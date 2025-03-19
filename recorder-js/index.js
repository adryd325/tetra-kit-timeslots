import { createSocket } from "dgram";
import childProcess from "child_process";
import zlib from "zlib";

const tplay = childProcess.spawn('./tplay');

const PORT = 42100;
const server = createSocket("udp4");

server.on("message", (msg, remoteInfo) => {
    onMessage(JSON.parse(msg), remoteInfo);
});

server.on("listening", () => {
    const serverAddress = server.address();

    console.log(`Listening at ${serverAddress.address}:${serverAddress.port}`);
});

server.bind(PORT);

let calls = new Map();

class Call {
    downlinkInfo = null;
    downlinkInfoStr = null;
    gssi = null;
    createTime = null;
    voiceTime = null;
    issis = new Set();
    // cmceTime = null;
    constructor() {
        this.createTime = Date.now();
    }
}

function getDownlinkInfo(usage, carrierNum, timeslot) {
    return ((timeslot - 1) << 18) | (carrierNum << 6) | usage;
}

function getUsage(value) {
    return value & 0b111111;
}

function getCarrierNum(value) {
    return (value >> 6) & 0b111111111111;
}

function getTimeslot(value) {
    return ((value >> 18) & 0b11) + 1;
}

function getDLInfoString(value) {
    return `usage:${getUsage(value)} carrier:${getCarrierNum(
        value
    )} timeslot:${getTimeslot(value)}`;
}

class DownlinkInfo {
    value = 0;
    constructor(usage, carrierNum, timeslot) {
        if (timeslot == 0) {
            throw new Error("Timeslot cannot be zero");
        }
        this.value = ((timeslot - 1) << 18) | (carrierNum << 6) | usage;
    }

    get usage() {
        return this.value & 0b111111;
    }

    get carrierNum() {
        return (this.value >> 6) & 0b111111111111;
    }

    get timeslot() {
        return ((this.value >> 18) & 0b11) + 1;
    }

    toString() {
        console.log(
            "usage:%i carrier:%i timeslot:%i",
            this.usage,
            this.carrierNum,
            this.timeslot
        );
    }
}

function releaseCid(cid) {
    if (calls.has(cid)) {
        calls.delete(cid);
    }
}

function getCallByCid(cid) {
    if (!calls.has(cid)) {
        calls.set(cid, new Call());
    }
    return calls.get(cid);
}

function getCallIdByDownlinkInfo(downlinkInfo) {
    for (let key of calls.keys()) {
        if (calls.get(key).downlinkInfo == downlinkInfo) {
            return key;
        }
    }
}

function getCallByDownlinkInfo(downlinkInfo) {
    return calls.get(getCallIdByDownlinkInfo(downlinkInfo));
}

function setGSSIForCid(cid, gssi) {
    // TTC Specific
    // if (gssi == 60002 || gssi == 60101 || (gssi >= 100000 && gssi <= 199999)) {
        getCallByCid(cid).gssi = gssi;
    // }
}

function onMessage(msg, remoteInfo) {
    switch (msg.service) {
        case "UPLANE":
            handleUplane(msg, remoteInfo);
            break;
        case "CMCE":
            handleCmce(msg);
            break;
    }
}

setInterval(cleanUp, 500);

function cleanUp() {
    for (let key of calls.keys()) {
        let call = calls.get(key);
        // Timeout old calls
        if (call.voiceTime == null && call.createTime + 1000 * 60 < Date.now()) {
            calls.delete(key);
        } else if (calls.voiceTime + 1000 * 20 < Date.now()) {
            calls.delete(key);
        }

        // Deduplicate calls using the same slot
        for (let key2 of calls.keys()) {
            let call2 = calls.get(key2);
            if (key == key2) break;
            if (call.voiceTime < call2.voiceTime) break;
            if (
                getCarrierNum(call2.downlinkInfo) == getCarrierNum(call.downlinkInfo) &&
                getTimeslot(call2.downlinkInfo) == getTimeslot(call.downlinkInfo)
            ) {
                console.log("Reused slot: ", call.downlinkInfoStr);
                calls.delete(key2);
            }
        }
    }
    console.log("Active calls: ", calls);
}

function handleUplane(msg, remoteInfo) {
    // for the sake of testing
    // msg["rx carrier nr"] = 518
    const call = getCallByDownlinkInfo(
        getDownlinkInfo(
            msg["downlink usage marker"],
            msg["rx carrier nr"],
            msg["tn"]
        )
    );
    console.log(msg)
    if (call) {
        // console.log(
        //     getDLInfoString(
        //         getDownlinkInfo(
        //             msg["downlink usage marker"],
        //             msg["rx carrier nr"],
        //             msg["tn"]
        //         )
        //     ) +
        //     " talkgroup:" +
        //     call.gssi
        // );
        call.voiceTime = Date.now();
        if (true || call.gssi == 101011) {
            let frame = Buffer.from(msg["frame"], "base64");
            zlib.inflate(frame, (err, buffer) => { 
                console.log("sent frame to tplay");       
                tplay.stdin.write(buffer)    
                tplay.stdin.write(Buffer.from(""))
                // tplay.stdin.end()   
                console.log(tplay.stdin)
             }); 
        }
    }
    return;
}

function handleCmce(msg, remoteInfo) {
    let cid;
    let call;
    switch (msg.pdu) {
        case "D-SETUP":
            cid = msg["call identifier"];
            call = getCallByCid(cid);
            call.downlinkInfo = getDownlinkInfo(
                msg["usage marker"],
                msg["allocation carrier number"],
                msg["allocation timeslot"]
            );
            call.downlinkInfoStr = getDLInfoString(call.downlinkInfo);
            setGSSIForCid(cid, msg["actual ssi"]);
            break;
        case "D-TX GRANTED":
            cid = msg["call identifier"];
            call = getCallByCid(cid);
            if (msg['transmitting party ssi']) {
                call.issis.add(msg['transmitting party ssi'])
                call.speaker = msg['transmitting party ssi']
            }
            break;
        case "D-TX CEASED":
            cid = msg["call identifier"];
            call = getCallByCid(cid);
            call.speaker = null
            if (msg['transmitting party ssi']) {
                call.issis.add(msg['transmitting party ssi'])
            }
            break;
        case "D-RELEASE":
            releaseCid(msg["call identifier"]);
            break;
    }
}
