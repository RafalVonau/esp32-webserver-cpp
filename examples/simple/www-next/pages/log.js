import React, { useEffect, useState } from 'react';
import SwitchSidebar from '../lib/switchSidebar';
import { Group, Title, Space, Box, Card, Text, ScrollArea, Select, useMantineColorScheme } from '@mantine/core';
import axios from 'axios'
import { showNotification } from '@mantine/notifications';
import { X } from 'tabler-icons-react';
import useIsMobile from "../lib/mobile.js"

function UTCtimestampToString2(t) {
    const d = new Date(Math.round(t*1000));
    const humanDateFormat = ("0" + d.getDate()).slice(-2) + "-" + ("0"+(d.getMonth()+1)).slice(-2) + "-" +d.getFullYear() + " " + ("0" + d.getHours()).slice(-2) + ":" + ("0" + d.getMinutes()).slice(-2) + ":" + ("0" + d.getSeconds()).slice(-2);
    return humanDateFormat;
}
    

// {"s":1696852135,"ms":74,"p":0,"x":"ECT","y":"updateCPUUsage","z":"CPU0 =  14%, CPU1 =  85%"}
function InfoItem({s, ms, p, x, y, z, c, im}) {    
    if (c === "dark") {
        let color = ["white", "green", "red"];

        return (<div style={{ whiteSpace: "pre-wrap", fontSize: im?"10px":"14px"}}>
            <span style={{color: "white"}}>{UTCtimestampToString2(s)}.{('00' + ms).slice(-3)}</span>&nbsp;
            <span style={{color: "gray"}}>{x}({y})</span>&nbsp;
            <span style={{color: color[p]}}>{z.replaceAll(';;',';').replaceAll(';','\n')}</span>
        </div>);
    } else {
        let color = ["black", "green", "red"];
        return (<div style={{ whiteSpace: "pre-wrap", fontSize: im?"10px":"14px"}}>
            <span style={{color: "black"}}>{UTCtimestampToString2(s)}.{('00' + ms).slice(-3)}</span>&nbsp;
            <span style={{color: "gray"}}>{x}({y})</span>&nbsp;
            <span style={{color: color[p]}}>{z.replaceAll(';;',';').replaceAll(';','\n')}</span>
        </div>);
    }
}



function Log({ }) {
    const title = "System log";
    const [infoItems, setInfoItems] = useState([]);
    const [value, setValue] = useState('Application');
    const { colorScheme, toggleColorScheme } = useMantineColorScheme();
    const { isMobile } = useIsMobile();

    const fetchData = async () => {
		try {
            let msg = [];
            if (value === "Application") {
                msg = await axios.get("api/log?type=0", {timeout:10000});
            } else {
                msg = await axios.get("api/log?type=1", {timeout:10000});
            }
            setInfoItems(msg.data);
        } catch(e) {
            console.error(e);
            showNotification({
                id: 'comerror',
				autoClose: 10000,
				title: 'Communication error',
				message: `Device is not responding!`,
				color: "red",
				icon: <X />,
			});
        }
	} 

    useEffect(() => {
        fetchData();
    }, [value])

    return (
        <SwitchSidebar pageContent={
            <>
                <Title order={1}>{title}</Title>
                <Space h='sm' />
                <Group>
                    <Text>Log source:</Text>
                    <Select data={["Application", "Sysrestore"]} value={value} onChange={setValue} />
                </Group>
                <Space h='sm' />
                <ScrollArea sx={{height: isMobile?"calc(100vh - 350px)":"calc(100vh - 200px)" }}>
                    {infoItems.map((i, index) => <InfoItem key={`log_item_${index}`} {...i} c={colorScheme} im={isMobile}/>)} 
                </ScrollArea>
            </>
        } />
    );
}

export default Log;