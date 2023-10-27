//import io from 'socket.io-client'

let socket = null;
let isConnected = false;


export default async function initSocket(fn) {
    if (isConnected) {
        fn(socket);
    } else {
        //await fetch('/api/socket')
//        socket = io();
//        socket.on('connect', () => {
//            console.log('connected :-)');
//            isConnected = true;
//            fn(socket);            
//        })
    }
}