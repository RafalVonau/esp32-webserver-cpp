const os = require("os");

export default function handler(req, res) {
    res.status(200).json([
        {d: "2020-08-07 15:55:48", u:"admin", a: "New firmware installed"},
        {d: "2020-08-10 14:20:43", u:"admin", a: "New firmware installed"},
        {d: "2022-02-16 11:14:55", u:"admin", a: "New firmware installed"},        
        {d: "2022-02-17 11:14:55", u:"admin", a: "VLAN changed"},        
        {d: "2022-02-18 11:14:55", u:"user", a: "IP changed"},        
    ]);
}

