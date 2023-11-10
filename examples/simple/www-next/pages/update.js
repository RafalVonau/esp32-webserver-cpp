import React, { useContext, useState } from "react";
import SwitchSidebar from "../lib/switchSidebar";
import {
	Group,
	Text,
	useMantineTheme,
	MantineTheme,
	Title,
	Space,
	Button,
	Loader,
	Center,
	Progress,
	Modal,
} from "@mantine/core";
import { Dropzone, DropzoneStatus, IMAGE_MIME_TYPE } from "@mantine/dropzone";
import { Check, Upload, X, FilePlus, RefreshAlert } from "tabler-icons-react";
import { useModals } from "@mantine/modals";
import { showNotification, updateNotification } from "@mantine/notifications";
import axios from "axios";

function sleep(ms) {
	return new Promise((resolve, reject) => {
		setTimeout(() => {
			resolve();
		}, ms);
	});
}

function toBuffer(ab) {
    const buf = Buffer.alloc(ab.byteLength);
    const view = new Uint8Array(ab);
    for (let i = 0; i < buf.length; ++i) {
        buf[i] = view[i];
    }
    return buf;
}

function Update() {
	const theme = useMantineTheme();
	const modals = useModals();
	const [progress, setProgress] = useState(0);
	const [progressModal, setProgressModal] = useState(false);

	const wsFirmwareUpdate = (bb) => {
		return new Promise(function (resolve, reject) {
			try {
				let binary = new Uint8Array(bb);
				console.log(binary);
				const ws = new WebSocket(`ws://${location.host}/ws`);
				const id = Math.round(new Date().getTime() / 1000);
				const maxDataSize = 4096;
				const total = binary.length;
				let pos = 0;
				let uprogress = 0;
				const cmd = `ota ${id} ${total}`;

				console.log(`Firmware update total=${total}`);

				let sendNextChunk = function () {
					if (pos < total) {
						let s = total - pos;
						if (s > maxDataSize) s = maxDataSize;
						const ab = new ArrayBuffer(s + 4);
						/* Fill packet */
						{
							let view = new DataView(ab);
							view.setUint32(0, id, true);
							for (let i = 0; i < s; ++i) {
								view.setUint8(4 + i, binary[pos + i]);
							}
						}
						ws.send(toBuffer(ab));
						pos += s;
					} else {
						// stop the progress bar
						//if (!argv.np) bar1.stop();
					}
				};

				ws.addEventListener("open", (event) => {
					setProgress((progress) => uprogress);
					console.time("ota");
					ws.send(cmd);
					sendNextChunk();
				});

				// Listen for messages
				ws.addEventListener("message", (event) => {
					let x = JSON.parse(event.data);
					console.log(x);
					if (x.cmd === "ota") {
						if (x.ret != 0) {
							console.log(`OTA upload error: ${x.ret}`);
							resolve(-1);
						}
						uprogress = Math.round((100.0 * x.val) / total);
						console.log(`Progress = ${uprogress} %`);
						setProgress((progress) => uprogress);
						if (x.val == total) {
							console.timeEnd("ota");
                            ws.close();
							resolve(0);
						} else {
							sendNextChunk();
						}
					}
				});
                ws.addEventListener("error", (e) => { reject('WebSocket error');})

			} catch (e) {
				console.error("UPSS", e);
				reject(e);
			}
		});
	};

	const openConfirmUpdateModal = (files) =>
		modals.openConfirmModal({
			title: "Do you really want to update the firmware?",
			children: <Text size="sm">Please confirm updating the firmware.</Text>,
			labels: { confirm: "Update", cancel: "Cancel" },
			closeOnConfirm: false,
			onCancel: () => console.log("Cancel"),
			onConfirm: async () => {
				modals.closeAll();
				const file = files[0];
				let error = "";
                console.log(file);
				try {
					setProgressModal(true);

					try {
						const buf = await file.arrayBuffer();
						error = await wsFirmwareUpdate(buf);
					} catch (e) {
						error = "Firmware update failed";
					}

					modals.closeAll();
					setProgress(0);
					setProgressModal(false);
					if (error == "") {
						showNotification({
							autoClose: 5000,
							title: "Firmware updated",
							message: ``,
							color: "green",
							icon: <Check />,
						});
						modals.closeAll();
						showNotification({
							id: "restart",
							loading: true,
							autoClose: false,
							disallowClose: true,
							title: "Restart",
							message: `The device is restarting...`,
							color: "yellow",
							icon: <RefreshAlert />,
						});
						while (true) {
							/* Wait for operation completed. */
							await sleep(500);
							try {
	        					const r = await axios.get("api/ping", { timeout: 500 });
								break;
							} catch (e) {
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
					} else {
						showNotification({
							autoClose: 5000,
							title: "Firmware update failed",
							message: error,
							color: "red",
							icon: <X />,
						});
					}
				} catch (error) {
					console.log(error);
					modals.closeAll();
					setProgress(0);
					setProgressModal(false);
					showNotification({
						autoClose: 5000,
						title: error.message,
						message: `Server response: ${error.response?.data?.error}`,
						color: "red",
						icon: <X />,
					});
				}
			},
		});

	return (
		<SwitchSidebar
			pageContent={
				<>
					<Modal
						opened={progressModal}
						closeOnConfirm={false}
						withCloseButton={false}
						closeOnEscape={false}
						closeOnClickOutside={false}
						title="Update progress"
					>
						<Progress
							value={progress}
							label={`${progress.toFixed(1)}%`}
							size="xl"
							radius="xl"
						/>
					</Modal>
					<Title order={1}>Firmware update</Title>
					<Space h="sm" />
					<Dropzone
						onDrop={(files) => openConfirmUpdateModal(files)}
						onReject={(files) => console.log("rejected files", files)}
					>
						<Group
							position="center"
							spacing="xl"
							style={{ minHeight: 220, pointerEvents: "none" }}
						>
							<Dropzone.Accept>
								<Upload
									size={50}
									color={
										theme.colors[theme.primaryColor][
											theme.colorScheme === "dark" ? 4 : 6
										]
									}
								/>
							</Dropzone.Accept>
							<Dropzone.Reject>
								<X
									size={50}
									color={theme.colors.red[theme.colorScheme === "dark" ? 4 : 6]}
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
									Attach one file with the bin extension
								</Text>
							</div>
						</Group>
					</Dropzone>
				</>
			}
		/>
	);
}

export default Update;
