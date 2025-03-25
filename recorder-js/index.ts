import { createSocket } from "dgram";
import zlib from "zlib";
import {
  Call,
  getCallByCid,
  getCallByDownlinkInfo,
  releaseCallByCid,
} from "./call.ts";
import { submitPlaying } from "./live.ts";

const PORT = 42100;
const server = createSocket("udp4");

server.on("message", (msg, remoteInfo) => {
  onMessage(JSON.parse(msg.toString()), remoteInfo);
});

server.on("listening", () => {
  const serverAddress = server.address();
  console.log(`Listening at ${serverAddress.address}:${serverAddress.port}`);
});

server.bind(PORT);

function onMessage(msg, remoteInfo) {
  switch (msg.service) {
    case "UPLANE":
      handleUplane(msg, remoteInfo);
      break;
    case "CMCE":
      handleCmce(msg, remoteInfo);
      break;
  }
}

function handleUplane(msg, remoteInfo) {
  const usage = msg["downlink usage marker"];
  const carrier = msg["rx carrier nr"];
  const timeslot = msg["etn"];
  if (timeslot != 4) {
    // console.log("\x1b[31m!! TIMESLOT NOT 4? !!\x1b[0m")
  } 
  const call = getCallByDownlinkInfo(usage, carrier, timeslot);
  let frame = Buffer.from(msg["frame"], "base64");
  if (call) {
    console.log(
      "TRAFFIC FRAME",
      `usage:${usage} carrier:${carrier} timeslot:${timeslot} rts:${msg["tn"]}`
    );
    zlib.inflate(frame, (err, buffer) => {
      call.submit(buffer);
      submitPlaying(call, buffer);
    });
  } else {
    console.log(
      "UNASSIGNED FRAME",
      `usage:${usage} carrier:${carrier} timeslot:${timeslot} rts:${msg["tn"]}`
    );
    // Listen to unassigned frames for debug
    // zlib.inflate(frame, (err, buffer) => {
    //   submitPlaying(null, buffer, true);
    // });
  }
  return;
}

function handleCmce(msg, remoteInfo) {
  let cid;
  let call: Call;
  switch (msg.pdu) {
    case "D-SETUP":
      console.log(
        msg.pdu.padStart(12),
        msg["call identifier"],
        msg["actual ssi"],
        msg["actual usage marker"],
        msg["allocation carrier number"],
        msg["allocation timeslot"]
      );
      cid = msg["call identifier"];
      call = getCallByCid(cid);
      call.duplex = !!msg["simplex/duplex selection"];
      call.setDownlinkInfo(
        msg["actual usage marker"],
        msg["allocation carrier number"],
        msg["allocation timeslot"]
      );
      call.gssi = msg["actual ssi"];
      call.addSsi(msg["actual ssi"]);
      if (msg["transmitting party ssi"]) {
        call.addSsi(msg["transmitting party ssi"]);
      }
      if (msg["calling party ssi"]) {
        call.addSsi(msg["calling party ssi"]);
      }
      break;
    case "D-TX GRANTED":
      console.log(
        msg.pdu.padStart(12),
        msg["call identifier"],
        msg["actual ssi"]
      );
      cid = msg["call identifier"];
      call = getCallByCid(cid);
      call.addSsi(msg["actual ssi"]);
      if (msg["transmitting party ssi"]) {
        call.addSsi(msg["transmitting party ssi"]);
      }
      if (msg["calling party ssi"]) {
        call.addSsi(msg["calling party ssi"]);
      }
      break;

    case "D-TX CEASED":
      console.log(
        msg.pdu.padStart(12),
        msg["call identifier"],
        msg["actual ssi"]
      );
      cid = msg["call identifier"];
      call = getCallByCid(cid);
      if (msg["transmitting party ssi"]) {
        call.addSsi(msg["transmitting party ssi"]);
      }
      break;
    case "D-RELEASE":
      console.log(
        msg.pdu.padStart(12),
        msg["call identifier"],
        msg["actual ssi"]
      );
      releaseCallByCid(msg["call identifier"]);
      break;
  }
}
