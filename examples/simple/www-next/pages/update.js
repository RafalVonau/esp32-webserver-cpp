import React, { useContext, useState } from 'react';
import SwitchSidebar from '../lib/switchSidebar';
import { Group, Text, useMantineTheme, MantineTheme, Title, Space, Button, Loader, Center, Progress, Modal } from '@mantine/core';
import { Dropzone, DropzoneStatus, IMAGE_MIME_TYPE } from '@mantine/dropzone';
import { Check, Upload, X, FilePlus, RefreshAlert } from 'tabler-icons-react';
import { useModals } from '@mantine/modals';
import { showNotification, updateNotification  } from '@mantine/notifications';
import axios from "axios";

function sleep(ms) {
    return new Promise((resolve, reject) => {setTimeout(() => {resolve();}, ms);});
}

function Update() {
    const theme = useMantineTheme();
    const modals = useModals();
    const [progress, setProgress] = useState(0);
    const [progressModal, setProgressModal] = useState(false);

    /*!
     * \brief Use event source to monitor firmware update.
     */
    const monitorFirmwareUpdate = (formData) => {
        return new Promise((resolve) => {
            const es = new EventSource('/api/events');
            let armed = 0;
            es.onopen = function(e) { console.log("Events Opened"); };
            es.onerror = function(e) {
                if (e.target.readyState != EventSource.OPEN) {
                    console.log("Events Closed");
                    resolve('Firmware update failed');
                }
            };
            es.addEventListener('cmd', function(r) {                
                let j = JSON.parse(r.data);
                console.log(j);
                if ("state" in j) {
                    if (j.state == "uploading") {
                        setProgress(progress => j.progress);
                        armed = 1;
                    } else if (j.state == "idle") {
                        es.close();
                        if (armed) {                            
                            resolve('');
                        } else {
                            resolve('The firmware update has not started!');
                        }
                    } else {  
                        /* Ups - error */                          
                        let error = j.message;
                        if (error == '') {
                            error = 'Firmware update failed';    
                        }
                        es.close();
                        resolve(error);                    
                    }
                }
            }, false);
            axios.post('api/firmware', formData, {headers: {"Content-Type": "multipart/form-data",} }).then((res) => {
                if (!res.data.ok) {
                    es.close();
                    resolve('Firmware update failed');
                }
            });
        });
    }

    /*!
     * \brief Use Axjos and poll method (500ms intervwl) to monitor firmware update.
     */
    const monitorFirmwareUpdateAxjos = async (formData) => {
        let armed = 0;
        let error = '';
        let res = await axios.post('api/firmware', formData, {headers: {"Content-Type": "multipart/form-data",} });
        if (res.data.ok) {
            while(true) {
                await sleep(500);
                let r = await axios.get('api/firmware')
                console.log(r);
                if (r.data.state == "uploading") {
                    /* Uploading in progress... */
                    setProgress(progress => r.data.progress);
                    armed = 1;
                } else if (r.data.state == "idle") {
                    /* done :-) */
                    if (armed) {
                        error = '';
                        break;
                    } else {
                        error = 'The firmware update has not started!';
                        break;
                    }
                } else {  
                    /* Ups - error */                          
                    error = r.data.message;
                    if (error == '') {
                        error = 'Firmware update failed';    
                    }
                    break;
                }
            }
        } else {
            error = 'Firmware update failed';
        }
        return error;
    }


    const openConfirmUpdateModal = (files) => modals.openConfirmModal({
        title: 'Do you really want to update the firmware?',
        children: (
            <Text size="sm">
                Please confirm updating the firmware.
            </Text>
        ),
        labels: { confirm: 'Update', cancel: 'Cancel' },
        closeOnConfirm: false,
        onCancel: () => console.log('Cancel'),
        onConfirm: async () =>  {
            modals.closeAll(); 
            const file = files[0];
            try {
                const formData = new FormData();
                formData.append("file", file);
                let error = '';
                setProgressModal(true);

                try {
                    //error = await monitorFirmwareUpdateAxjos(formData);
                    error = await monitorFirmwareUpdate(formData);
                } catch (e) {
                    error = 'Firmware update failed';
                }

                modals.closeAll(); 
                setProgress(0);
                setProgressModal(false);
                if (error == '') {
                    showNotification({
                        autoClose: 5000,
                        title: 'Firmware updated',
                        message: ``,
                        color: 'green',
                        icon: <Check />,
                    });
                    modals.openConfirmModal({
                        title: 'Do you want to restart the device?',
                        children: (
                            <Text size="sm">
                                Please confirm device restart
                            </Text>
                        ),
                        labels: { confirm: 'Restart', cancel: 'Cancel' },
                        closeOnConfirm: false,
                        onCancel: () => console.log('Cancel'),
                        onConfirm: async () =>  {
                            console.log('Restart');
                            try {
                                let r = await axios.get('api/action', {params: { action: 'restart'}})
                            } catch (e) {
                                console.log(e);
                            }
                            modals.closeAll(); 
                            showNotification({
                                id: 'restart',
                                loading: true,
                                autoClose: false,
                                disallowClose: true,
                                title: 'Restart',
                                message: `The device is restarting...`,
                                color: 'yellow',
                                icon: <RefreshAlert />,
                            });
                            while(true) {
                                /* Wait for operation completed. */
                                await sleep(500);
                                try {
                                    const r = await axios.get('api/firmware',  {timeout: 500});
                                    break;
                                } catch(e) {
                                    console.log(`Not yet...`);
                                }
                            }
                            updateNotification({
                                id: "restart",
                                color: "yellow",
                                title: "Restart",
                                message: `Restart done :-)`,
                                icon: <RefreshAlert />,
                                autoClose: 4000,
                                disallowClose: false,
                            });
                        }
                    });
                } else {
                    showNotification({
                        autoClose: 5000,
                        title: 'Firmware update failed',
                        message: error,
                        color: 'red',
                        icon: <X />,
                    });
                }
            } catch (error) {
                console.log(error);
                modals.closeAll();
                showNotification({
                    autoClose: 5000,
                    title: error.message,
                    message: `Server response: ${error.response?.data?.error}`,
                    color: 'red',
                    icon: <X />,
                });
            }
        }
    });

    return (
        <SwitchSidebar pageContent={
            <>
            <Modal opened={progressModal} closeOnConfirm={false} withCloseButton={false} closeOnEscape={false} closeOnClickOutside={false} title= 'Update progress'>
                <Progress value={progress} label={`${progress.toFixed(1)}%`} size="xl" radius="xl"/>
            </Modal>
                <Title order={1}>Firmware update</Title>
                <Space h='sm' />
                <Dropzone
                    onDrop={(files) => openConfirmUpdateModal(files)}
                    onReject={(files) => console.log('rejected files', files)}
                >
                    <Group position="center" spacing="xl" style={{ minHeight: 220, pointerEvents: 'none' }}>
                        <Dropzone.Accept>
                            <Upload
                                size={50}
                                color={theme.colors[theme.primaryColor][theme.colorScheme === 'dark' ? 4 : 6]}
                            />
                        </Dropzone.Accept>
                        <Dropzone.Reject>
                            <X
                                size={50}
                                color={theme.colors.red[theme.colorScheme === 'dark' ? 4 : 6]}
                            />
                        </Dropzone.Reject>
                        <Dropzone.Idle>
                            <FilePlus size={50} />
                        </Dropzone.Idle>

                        <div>
                            <Text size="xl" inline>
                                Drag files here or click to select files
                            </Text>
                            <Text size="sm" color="dimmed" inline mt={7}>
                                Attach one file with the tar.gz extension
                            </Text>
                        </div>
                    </Group>
                </Dropzone>
            </>

        } />
    );
}

export default Update;