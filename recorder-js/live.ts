import * as fs from "fs";
import { Call, calls, sortCallsByActivity } from "./call.ts";
import child_process from "child_process";

let priorityGssis = new Map();
try {
  const priorityCsv = fs
    .readFileSync("./live_priority.csv", "utf-8")
    .replaceAll("\r", "")
    .split("\n");
  let values;
  for (let i = 0; i < priorityCsv.length; i++) {
    if (i == 0) continue;
    values = priorityCsv[i].split(",");
    priorityGssis.set(parseInt(values[0]), parseInt(values[1]));
  }
} catch (e) {
  console.log(e);
  console.log("Failed to load priority list, not using priority sorting.");
}

let silence = fs.readFileSync("./silence.acelp");
let decoder = child_process.spawn("./play");

let lastTraffic = 0;

const closeInterval = setInterval(() => {
  if (lastTraffic != 0 && lastTraffic + 250 < Date.now()) {
    // Flush buffer
    decoder.stdin.write(silence);
    lastTraffic = 0;
  }
}, 25);

function selectSuitable(sortedCalls) {
  let callsList = sortedCalls;
  // Select a suitable
  let priorityCalls = sortedCalls
    .filter((call) => priorityGssis.has(call.gssi))
    .filter((call) => call.voiceTime + 2000 > Date.now())
    .sort((a, b) => {
      let apri = priorityGssis.get(a.gssi);
      let bpri = priorityGssis.get(b.gssi);
      if (apri < bpri) {
        return 1;
      } else if (apri > bpri) {
        return -1;
      }
      return 0;
    });

  if (priorityCalls.length > 0) {
    callsList = priorityCalls;
  }

  for (let i = 0; i < callsList.length; i++) {
    if (callsList[i].voiceTime + 2000 > Date.now() && !callsList[i].duplex) {
      callsList[i].play = true;
      break;
    }
    // Don't loop over every call
    if (callsList[i].voiceTime + 2000 < Date.now()) {
      break;
    }
  }
}

export function selectPlaying() {
  // sort most recent to least recent
  let sortedCalls = [...calls].sort(sortCallsByActivity);
  let nowPlaying = sortedCalls.find((a) => a.play);
  if (!nowPlaying) {
    selectSuitable(sortedCalls);
  } else if (nowPlaying && nowPlaying.voiceTime + 5000 < Date.now()) {
    // select new call to play
    nowPlaying.play = false;
    selectSuitable(sortedCalls);
  }
  // No change
}

export function submitPlaying(call: Call, buffer: Buffer, force?: boolean) {
  // Shhh
  // return;
  selectPlaying();
  if (force || call.play) {
    lastTraffic = Date.now();
    console.log(
      `\x1b[1;35m${"NOW PLAYING".padStart(16)}\x1b[0m cid:${call.cid} gssi:${
        call.gssi
      } `
    );
    decoder.stdin.write(buffer);
  }
}
