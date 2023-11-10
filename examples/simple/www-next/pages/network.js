import React, { useEffect, useState } from "react";
import SwitchSidebar from "../lib/switchSidebar";
import {
	Button,
	Title,
	Space,
	Affix,
	Transition,
	Center,
	Card,
	Group,
	Text,
	SegmentedControl,
	MultiSelect,
	InputBase,
} from "@mantine/core";
import axios from "axios";
import { useForm } from "@mantine/form";
import { X, Check, RefreshAlert } from "tabler-icons-react";
import { useModals } from "@mantine/modals";
import { showNotification, updateNotification } from "@mantine/notifications";
import MaskedInput from "react-text-mask";

function sleep(ms) {
    return new Promise((resolve, reject) => {setTimeout(() => {resolve();}, ms);});
}

const props = {
	guide: false,
	placeholderChar: "\u2000",
	mask: (value) => Array(value.length).fill(/[\d.]/),
	pipe: (value) => {
		if (value === "." || value.endsWith("..")) return false;

		const parts = value.split(".");

		if (
			parts.length > 4 ||
			parts.some((part) => part === "00" || part < 0 || part > 255)
		) {
			return false;
		}

		return value;
	},
};

function Network({}) {
	const modals = useModals();
	const [netsettings, setNetSettings] = useState();
	const [loading, setLoading] = useState(true);
	const networkForm = useForm({
		initialValues: {
			net: {
				ip: "",
				netmask: "",
				gateway: "",
				dhcp: "STATIC",
				ntps: [],
				ntp: "NONTP",
			},
		},
	});

	const [data, setData] = useState([
		{
			value: "0.debian.pool.ntp.org",
			label: "0.debian.pool.ntp.org",
			group: "Debian",
		},
		{
			value: "1.debian.pool.ntp.org",
			label: "1.debian.pool.ntp.org",
			group: "Debian",
		},
		{
			value: "2.debian.pool.ntp.org",
			label: "2.debian.pool.ntp.org",
			group: "Debian",
		},
		{
			value: "3.debian.pool.ntp.org",
			label: "3.debian.pool.ntp.org",
			group: "Debian",
		},
		{
			value: "time.google.com",
			label: "time.google.com",
			group: "Google Public NTP",
		},
		{
			value: "time1.google.com",
			label: "time1.google.com",
			group: "Google Public NTP",
		},
		{
			value: "time2.google.com",
			label: "time2.google.com",
			group: "Google Public NTP",
		},
		{
			value: "time3.google.com",
			label: "time3.google.com",
			group: "Google Public NTP",
		},
		{
			value: "time4.google.com",
			label: "time4.google.com",
			group: "Google Public NTP",
		},
		{
			value: "time.cloudflare.com",
			label: "time.cloudflare.com",
			group: "Cloudflare NTP",
		},
		{
			value: "time.facebook.com",
			label: "time.facebook.com",
			group: "Facebook NTP",
		},
		{
			value: "time1.facebook.com",
			label: "time1.facebook.com",
			group: "Facebook NTP",
		},
		{
			value: "time2.facebook.com",
			label: "time2.facebook.com",
			group: "Facebook NTP",
		},
		{
			value: "time.apple.com",
			label: "time.apple.com",
			group: "Apple NTP server",
		},
	]);

	const checkOnTheList = (v) => {
		let ok = false;
		for (const x in data) {
			if (x.value == v) {
				ok = true;
				break;
			}
		}
		if (!ok) {
			console.log(`${v} is not on the list - add one`);
			setData( (data) => [ ...data, {value: v, label: v, group: "Custom" }]);
		}
	}


	const loadData = (res) => {
		let mydata = { ...res.data };
		if ("ntps" in res.data) {
			mydata.ntps = res.data.ntps.split(" ");
			mydata.ntps.map( (x) => checkOnTheList(x) );
		}
		console.log(mydata);
		const values = { net: { ...mydata } };
		networkForm.setValues(values);
		networkForm.resetDirty(values);
		setNetSettings(mydata);
	}

	const fetchData = async () => {
		try {
			const res = await axios.get("/api/network", {timeout: 2000});
			loadData(res);
			setLoading(false);
		} catch(e) {
			showNotification({
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
	}, []);

	async function applyChanges() {
		const data = networkForm.getInputProps("net").value;
		try {
			let mydata = { ...data };
			if ("ntps" in data) {
				mydata.ntps = data.ntps.join(" ");
			}
			console.log({ mydata });
			let res = await axios.get(`/api/network`, { params: mydata, timeout: 5000 });
			showNotification({
				autoClose: 5000,
				title: "Network updated",
				message: ``,
				color: "green",
				icon: <Check />,
			});
			loadData(res);
		} catch (error) {
			console.log(error);
			showNotification({
				autoClose: 5000,
				title: error.message,
				message: `Server response: ${error.response?.data?.error}`,
				color: "red",
				icon: <X />,
			});
			//if ((error.response) && (error.response.status == 401)) {

			//}
		}
	}

	const discardChanges = (index) => {
		modals.openConfirmModal({
			title: "Discard chnages",
			centered: true,
			children: (
				<Text size="sm">Are you sure you want to discard changes?</Text>
			),
			labels: { confirm: "Discard", cancel: "Cancel" },
			confirmProps: { color: "red" },
			onCancel: () => {},
			onConfirm: () => {
				const values = { net: { ...netsettings } };
				console.log(values);
				networkForm.setValues(values);
				networkForm.resetDirty(values);
			},
		});
	};

	const doRestart = () => {
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
					let r = await axios.get('api/restart')
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
						const r = await axios.get('api/ping',  {timeout: 500});
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
	}



	return (
		<SwitchSidebar
			pageContent={
				<>
					<Title order={1}>Network settings</Title>
					<Space h="sm" />
					<Text hidden={!loading}>Loading...</Text>
					{/* <Center> */}
					{/* <Paper shadow="xs" p="md" withBorder> */}
					{/* <Box sx={{ width: '25vw' }} mx="auto"> */}
					<form hidden={loading}
						onSubmit={networkForm.onSubmit((values) => console.log(values))}
					>
						<SegmentedControl
							color="blue"
							data={[
								{ label: "STATIC", value: "STATIC" },
								{ label: "DHCP Client", value: "DHCP" },
								{ label: "DHCP Server", value: "DHCPS" },
							]}
							{...networkForm.getInputProps("net.dhcp")}
						/>
						<InputBase
							label="IP address"
							placeholder="192.168.1.1"
							component={MaskedInput}
							{...props}
							{...networkForm.getInputProps("net.ip")}
						/>

						<InputBase
							label="Netmask"
							placeholder="255.255.255.0"
							mt="xs"
							component={MaskedInput}
							{...props}
							{...networkForm.getInputProps("net.netmask")}
						/>
						<InputBase
							label="Gateway"
							placeholder="192.168.1.1"
							component={MaskedInput}
							{...props}
							mt="xs"
							{...networkForm.getInputProps("net.gateway")}
						/>
						<Space h="sm" />
						<SegmentedControl
							color="blue"
							data={[
								{ label: "NTP Disabled", value: "NONTP" },
								{ label: "NTP Enabled", value: "NTP" },
							]}
							{...networkForm.getInputProps("net.ntp")}
						/>
						<MultiSelect
							label="NTP servers"
							data={data}
							placeholder="Select or add custom NTP servers"
							searchable
							creatable
							getCreateLabel={(query) => `+ Create ${query}`}
							onCreate={(query) => {
								const item = { value: query, label: query };
								setData((current) => [...current, item]);
								return item;
							}}
							mt="xs"
							mb="md"
							{...networkForm.getInputProps("net.ntps")}
						/>
					</form>
					<Space h="md" />
					<Button hidden={loading} onClick={doRestart}>Restart</Button>
					<Affix position={{ bottom: 20, right: 20, left: 20 }}>
						<Transition transition="slide-up" mounted={networkForm.isDirty()}>
							{(transitionStyles) => (
								<Center>
									<Card
										shadow="sm"
										p="lg"
										radius="md"
										withBorder
										style={transitionStyles}
									>
										<Group>
											<Text>You have unsaved changes</Text>
											<Button
												color="red"
												variant="light"
												onClick={discardChanges}
											>
												Discard
											</Button>
											<Button color="green" onClick={applyChanges}>
												Apply
											</Button>
										</Group>
									</Card>
								</Center>
							)}
						</Transition>
					</Affix>

					{/* </Box> */}

					{/* <Code block mt="md">{JSON.stringify(networkForm.values, null, 2)}</Code> */}
					{/* </Paper> */}
					{/* </Center> */}
				</>
			}
		/>
	);
}

export default Network;
