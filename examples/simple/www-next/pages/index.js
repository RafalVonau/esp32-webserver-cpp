import React, { useEffect, useState } from 'react';
import SwitchSidebar from '../lib/switchSidebar';
import { Group, Title, Space, Box, Card, Text } from '@mantine/core';
import initSocket from "../lib/_socket"
import axios from 'axios'
import { showNotification } from '@mantine/notifications';
import { X } from 'tabler-icons-react';

function InfoItem(props) {
    return (<Box>
        <Card shadow="sm" p="md" mb="sm" radius="md" withBorder>
            <    Group position="apart">
                <Text weight={500}>{props.k}</Text>
                <Text weight={900}>{props.v}</Text>
            </Group>
        </Card>
    </Box>
    );
}

function InfoPage({ }) {
    const [infoItems, setInfoItems] = useState('Loading...');

    function updateData(msg) {
        setInfoItems(msg.map((i, index) => { 
            if (String(i.v).startsWith('Linux')) {
                i.v = i.v.split(" ").slice(0,3).join(" ");
            }
            return (<InfoItem key={`home_item_${index}`} k={i.k} v={i.v} ></InfoItem>); 
        }));
    }

    const fetchData = async () => {
		try {
            const msg = await axios.get("api/info", {timeout:1000});
            updateData(msg.data);
        } catch(e) {
            console.error(e);
            if ((e.response) && (e.response.status === 401)) return;
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
        /* Fast first time fetch */
        fetchData();
        const intervalId = setInterval(async () => {
            fetchData();
        }, 1000);
        return () => {
            clearInterval(intervalId);
        }
    }, [])

    return (<>{infoItems}</>);
}


function Home({ }) {
    const title = "System information";

    return (
        <SwitchSidebar pageContent={
            <>
                <Title order={1}>{title}</Title>
                <Space h='sm' />
                <InfoPage />
            </>
        } />
    );
}

export default Home;