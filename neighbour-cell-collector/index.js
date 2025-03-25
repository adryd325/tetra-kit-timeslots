import { createSocket } from "dgram";
const PORT = 42100;
const server = createSocket("udp4");
import * as fs from "fs";



server.on("message", (msg, remoteInfo) => {
    onMessage(JSON.parse(msg), remoteInfo);
});

server.on("listening", () => {
    const serverAddress = server.address();

    console.log(`Listening at ${serverAddress.address}:${serverAddress.port}`);
});

server.bind(PORT);

const celldb = JSON.parse(fs.readFileSync("./cells.json", "utf-8"));

function offset(offs) {
    switch (offs) {
        case 0:
            return 0
        case 1:
            return 6250
        case 2:
            return -6250
        case 3:
            return 12500
    }
}

const duplexSpacingTable = 
    [10,7,0,8,5,9.5]


function getOrDefault(val, def) {
    if (val == undefined) {
        return def
    }
    return val
}

function onMessage(msg, remoteInfo) {
    if (msg.service == "MLE" && msg.pdu == "D-NWRK-BROADCAST") {
        console.log(msg)
        for (let index = 0; index < msg["number of neighbour cells"]; index++) {
            const cell_arr = msg["cell " + index];
            let cell = {}
            cell_arr.forEach(kvp => {cell[Object.keys(kvp)[0]] = Object.values(kvp)[0]})
            console.log(cell)
            if (!celldb[cell.LA]) {
                celldb[cell.LA] = {}
            }
            celldb[cell.LA] = Object.assign(cell, celldb[cell.LA])
            fs.writeFileSync("./cells.json", JSON.stringify(celldb,null,2))
        }
    }
}