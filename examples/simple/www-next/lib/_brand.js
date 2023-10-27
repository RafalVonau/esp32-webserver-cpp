import React from 'react';
import { Group, ActionIcon, useMantineColorScheme, Box, UnstyledButton, Center, ThemeIcon } from '@mantine/core';
import { Sun, IconMoonStars, IconSun } from 'tabler-icons-react';

export function Brand() {
  const { colorScheme, toggleColorScheme } = useMantineColorScheme();
  const dark = (colorScheme === 'dark');

  return (
        <Center>
        <UnstyledButton onClick={toggleColorScheme} >
        <ThemeIcon size="md" variant="default" mr={10}>
        <Sun></Sun>
        </ThemeIcon>
        </UnstyledButton>
        </Center>
  );
}