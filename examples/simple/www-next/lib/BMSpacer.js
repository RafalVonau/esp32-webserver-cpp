import { Box } from "@mantine/core";

export default function BMSpacer() {
    return (
        <Box h="calc(51px + max(calc(env(safe-area-inset-top) - 10px), 16px))"></Box>
    )
}