import "../styles/globals.css";
import {
	MantineProvider,
	ColorSchemeProvider,
	Modal,
	Select,
	PasswordInput,
	Space,
	Group,
	Button,
	TextInput,
} from "@mantine/core";
import { SpotlightProvider } from "@mantine/spotlight";
import {
	Home as HomeIcon,
	Search,
	ArrowRightCircle,
	Users,
	Logout,
	ArrowsSplit2,
	Upload,
	History,
	Network,
	Check,
	X,
	Plant,
	Article
} from "tabler-icons-react";
import { useRouter } from "next/router";
import { ModalsProvider } from "@mantine/modals";
import { useForm } from "@mantine/form";
import { NotificationsProvider } from "@mantine/notifications";
import { useHotkeys, useLocalStorage } from "@mantine/hooks";
import { createContext, useEffect, useRef, useState } from "react";
import { showNotification } from "@mantine/notifications";
import axios from "axios";
import Head from "next/head";

function MyApp({ Component, pageProps }) {
	const router = useRouter();
	const [colorScheme, setColorScheme] = useLocalStorage({
		key: "mantine-color-scheme",
		defaultValue: "dark",
		getInitialValueInEffect: true,
	});

	const toggleColorScheme = () =>
		setColorScheme(colorScheme === "dark" ? "light" : "dark");

	useHotkeys([["mod+J", () => toggleColorScheme()]]);

	const actions = [
		{
			title: "Home",
			description: "System information",
			onTrigger: () => router.push("/"),
			icon: <HomeIcon size={18} />,
			group: "Navigation",
		},
		{
			title: "Network",
			description: "Network settings",
			onTrigger: () => router.push("/network"),
			icon: <Network size={18} />,
			group: "Navigation",
		},
		{
			title: "Firmware",
			description: "Firmware update",
			onTrigger: () => router.push("/update"),
			icon: <Upload size={18} />,
			group: "Navigation",
		},
		{
			title: "Log",
			description: "Show device log",
			onTrigger: () => router.push("/log"),
			icon: <Article size={18} />,
			group: "Navigation",
		}

	];
	return (<>
		<Head>
			<title>Express test page</title>
			<meta name="viewport" content="width=device-width; initial-scale=1.0; maximum-scale=1.0, user-scalable=0; viewport-fit=cover" />
			<meta name="mobile-web-app-capable" content="yes" />
			<meta name="apple-mobile-web-app-capable" content="yes" />
			<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent" />
			<link rel="manifest" href="manifest.json" />
		</Head>
		<ColorSchemeProvider
			colorScheme={colorScheme}
			toggleColorScheme={toggleColorScheme}
		>
			<MantineProvider
				theme={{ colorScheme }}
				withGlobalStyles
				withNormalizeCSS
			>
				<SpotlightProvider
					shortcut={["mod + P", "mod + K", "/"]}
					actions={actions}
					searchIcon={<Search size={18} />}
					searchPlaceholder="Search..."
					nothingFoundMessage="Nothing found..."
				>
					<ModalsProvider>
						<NotificationsProvider>
							<Component {...pageProps} />
						</NotificationsProvider>
					</ModalsProvider>
				</SpotlightProvider>
			</MantineProvider>
		</ColorSchemeProvider>
	</>);
}

export default MyApp;
