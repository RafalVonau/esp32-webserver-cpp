import React, { useState } from "react";
import { AppShell, Navbar, useMantineTheme, Header, MediaQuery, Burger, Text, Button, Flex, Footer, Box } from "@mantine/core";
import { MainLinks } from "./_mainLinks";
import { Brand } from "./_brand";
import { useMediaQuery } from "@mantine/hooks";
import useIsMobile from "./mobile.js"

function SwitchSidebar({ pageContent }) {
	const theme = useMantineTheme();
	// const isMobile = useMediaQuery('(max-width: 600px)');
	const { isMobile } = useIsMobile();
	console.log("Render sidebar");
	return (
		<AppShell
			header={<>
				<Header height={60} p="md" mt="calc(env(safe-area-inset-top))">
					<div style={{ display: 'flex', alignItems: 'center', height: '100%' }}>
						<Brand></Brand>						
						<Text weight={900}>Express test</Text>
					</div>
				</Header>
				</>
			}
			navbar= {
				<Navbar hidden={isMobile} width={{ base: isMobile?0:200 }}>
				<MainLinks left></MainLinks>
				</Navbar>
			}
			footer={ isMobile && 
				<><Footer height={66} p="sm" style={{display: 'flex', alignItems: 'center'}}>
						<MainLinks></MainLinks>
				</Footer>
				</>
			}
		>
			{pageContent}
		</AppShell>
	);
}

export default SwitchSidebar;
