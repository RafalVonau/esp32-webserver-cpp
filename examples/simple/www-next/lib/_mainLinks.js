import React, { useState } from 'react';
import { GitPullRequest, AlertCircle, Messages, Database, Users, ArrowRightCircle, Search, Home, ArrowsSplit2, Upload, History, Network, Plant, Settings, Tex, Article } from 'tabler-icons-react';
import { ThemeIcon, UnstyledButton, Group, Text, Flex, Center, Affix } from '@mantine/core';
import { useRouter } from 'next/router';
import { openSpotlight } from '@mantine/spotlight';
import BMSpacer from "./BMSpacer";

function MainLink({ icon, color, label, navPath, left, selected}) {
  function handleClick() {
    if (navPath == ':spotlight') {
      openSpotlight();
    } else {
      router.push(navPath);
    }    
  }
  const router = useRouter();
  return (
    <UnstyledButton
      sx={(theme) => ({
        display: 'block',
        width: '100%',
        padding: theme.spacing.xs,
        borderRadius: theme.radius.sm,
        color: theme.colorScheme === 'dark' ? theme.colors.dark[0] : theme.black,
        backgroundColor: selected?theme.colorScheme === 'dark' ? theme.colors.dark[6] : theme.colors.gray[0]:undefined,
        '&:hover': {
          backgroundColor:
            theme.colorScheme === 'dark' ? theme.colors.dark[6] : theme.colors.gray[0],
        },
      })} onClick={handleClick}
    >
      {left ? <Group p={5} m={0}>
        <Center>
          {icon}
        </Center>
        <Text>{label}</Text>
      </Group> :
        <Center>
          {icon}
        </Center>
      }
    </UnstyledButton>
  );
}

const data = [
  { icon: <Home size={24} />, color: 'blue', label: 'Info', navPath: '/' },
  { icon: <Network size={24} />, color: 'cyan', label: 'Network', navPath: '/network' },
  { icon: <Upload size={24} />, color: 'violet', label: 'Firmware', navPath: '/update' },
  { icon: <Article size={24} />, color: 'blue', label: 'Log', navPath: '/log' },
];

export function MainLinks({ left }) {
  const router = useRouter();
  const links = data.map((link, i) => <MainLink {...link} selected={router.asPath === link.navPath} left={left} key={link.label} />);
  return left?<Group m={10} p={0} spacing={0}>{links}</Group>:<div style={{ width: '100%' }}> <Affix position={{ bottom: 0, right: 0, left: 0 }} zIndex={1000}>
  <Flex justify="space-around" align="center" sx={(theme) => ({
      padding: '10px 6px',
      borderTop: `1px solid ${theme.colors.gray[8]}`,
      backgroundColor: theme.colors.dark[7],
      paddingBottom: 'max(calc(env(safe-area-inset-top) - 10px), 10px)'
  })}>{links}</Flex></Affix><BMSpacer/></div>;
}