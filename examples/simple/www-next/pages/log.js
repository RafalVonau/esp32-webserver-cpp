import React, { useEffect, useState } from 'react';
import SwitchSidebar from '../lib/switchSidebar';
import { Group, Title, Space, ScrollArea, Select, MultiSelect, useMantineColorScheme, ActionIcon } from '@mantine/core';
import axios from 'axios'
import { showNotification } from '@mantine/notifications';
import { X, Reload } from 'tabler-icons-react';
import useIsMobile from "../lib/mobile.js"

function UTCtimestampToString2(t) {
    const d = new Date(Math.round(t));
    const ms = t%1000;
    const humanDateFormat = (("0" + d.getHours()).slice(-2) + ":" + ("0" + d.getMinutes()).slice(-2) + ":" + ("0" + d.getSeconds()).slice(-2) + "." + ("00" + ms).slice(-3));
    return humanDateFormat;
}
    
const loglevelTable = ["VERBOSE", "DEBUG", "INFO", "WARNING", "ERROR"];


// { "c": "0;32", "t": 1, "n": "753", "i": "WSM", "m": "This is ESP32 chip with 2 CPU cores, WiFi/BT/BLE, "}
function InfoItem({ c, t, n, i, m, im }) {    
    if (c === "dark") {
        let color = ["gray", "white", "green", "red"];

        return (<div style={{ whiteSpace: "pre-wrap", fontSize: im?"10px":"14px"}}>
            <span style={{color: "white"}}>{UTCtimestampToString2(n)}</span>&nbsp;
            <span style={{color: "gray"}}>{i}</span>&nbsp;
            <span style={{color: color[t]}}>{m.replaceAll(';;',';').replaceAll(';','\n')}</span>
        </div>);
    } else {
        let color = ["gray", "black", "green", "red"];
        return (<div style={{ whiteSpace: "pre-wrap", fontSize: im?"10px":"14px"}}>
            <span style={{color: "black"}}>{UTCtimestampToString2(n)}</span>&nbsp;
            <span style={{color: "gray"}}>{i}</span>&nbsp;
            <span style={{color: color[t]}}>{m.replaceAll(';;',';').replaceAll(';','\n')}</span>
        </div>);
    }
}



function Log({ }) {
    const title = "System log";
    const [infoItems, setInfoItems] = useState([]);
    const [systems, setSystems] = useState([]);
    const [system, setSystem] = useState([]);
    const [level, setLevel] = useState(0);
    const { colorScheme, toggleColorScheme } = useMantineColorScheme();
    const { isMobile } = useIsMobile();


    function parseExtendedLogLine(logLine) {
        const regex = /\u001b\[(\d+;\d+)m(\w+) \((\d+)\) (\w+): (.+?)\u001b\[0m/;
        const match = logLine.match(regex);
        const level = ['V', 'D', 'I', 'W', 'E'];
        if (match) {
            let [, c, t, n, i, m] = match;
            t = level.indexOf(t);
            return { c, t, n, i, m };
        } else {
            return null; // Jeśli nie udało się dopasować
        }
    }

    const fetchData = async () => {
		try {
            const msg = await axios.get("api/log", {timeout:10000});
            let lines = msg.data.replaceAll('\r\n','\n').replaceAll('\n\r','\n').split('\n').filter((f) => f !== "").map((v) => parseExtendedLogLine(v)).filter((e) => e !== null);
            console.log(lines);
            setInfoItems(lines);
            /* Extract system names */
            let set = new Set();
            for (let x of lines) {
                set.add(x.i);
            }
            setSystems((Array.from(set)).sort((a,b) => (''+a).toUpperCase() < (''+b).toUpperCase() ? -1 : 1));
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

    const setLogLevel = (v) => {
        setLevel(loglevelTable.indexOf(v));
    }

    useEffect(() => {
        fetchData();
    }, [])

    return (
        <SwitchSidebar pageContent={
            <>
                <Title order={1}>{title}</Title>
                <Space h='sm' />
                <Group>
                    <Select label="Filter by level:" data={loglevelTable} value={loglevelTable[level]} onChange={setLogLevel} />
                    <MultiSelect label="Filter by system:" data={systems} value={system} onChange={setSystem} />
                    <ActionIcon onClick={fetchData} title="Reload log" mt="xl" size="lg" variant="filled"><Reload size="1rem" /></ActionIcon>
                </Group>
                <Space h='sm' />
                <ScrollArea sx={{height: isMobile?"calc(100vh - 350px)":"calc(100vh - 225px)" }}>
                    {infoItems.filter((v) => v.t >= level).filter((v) => (system.length == 0) || system.includes(v.i)).map((i, index) => <InfoItem key={`log_item_${index}`} {...i} c={colorScheme} im={isMobile}/>)} 
                </ScrollArea>
            </>
        } />
    );
}

export default Log;