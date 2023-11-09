import {
    TextInput,
    PasswordInput,
    Checkbox,
    Anchor,
    Paper,
    Title,
    Text,
    Container,
    Group,
    Button,
} from '@mantine/core';
import { useForm } from '@mantine/form';
import { showNotification } from '@mantine/notifications';
import { X } from 'tabler-icons-react';
import axios from 'axios';
import { useRouter } from 'next/router';
import { useState } from 'react';

export default function AuthenticationTitle() {
    const form = useForm({
        user: '',
        password: '',
    });

    const router = useRouter();
    const [loading, setLoading] = useState(false);



    async function login() {
        setLoading(true);
        try {
            const { data } = await axios.post('/api/login', { user: form.values.user, password: form.values.password });
            router.push('/');
        } catch (error) {
            showNotification({
                title: 'Login error',
                message: error.response.data.ui,
                color: 'red',
                icon: <X />,
            });
            setLoading(false);
        }
    }

    return (
        <Container size={420} my={40}>
            <Title
                align="center"
                sx={(theme) => ({ fontFamily: `Greycliff CF, ${theme.fontFamily}`, fontWeight: 900 })}
            >
                Login page
            </Title>
            <Text color="dimmed" size="sm" align="center" mt={5}>
                Welcome back!
            </Text>

            <Paper withBorder shadow="md" p={30} mt={30} radius="md">
                <TextInput label="User name" placeholder="Enter user name" required {...form.getInputProps('user')} />
                <PasswordInput label="Password" placeholder="Enter password" required {...form.getInputProps('password')} />
                <Group position="apart" mt="lg">
                    <Checkbox label="Remember me" />
                </Group>
                <Button fullWidth mt="xl" onClick={login} loading={loading}>
                    Log in
                </Button>
            </Paper>
        </Container>
    );
}