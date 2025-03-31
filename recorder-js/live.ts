import * as fs from "fs";
import { Call, calls, sortCallsByActivity } from "./call.ts";
import child_process from "child_process";

let silence = fs.readFileSync("./silence.acelp");
let c = child_process.spawn("./play");

let lastTraffic = 0;

const closeInterval = setInterval(() => {
  if (lastTraffic != 0 && lastTraffic + 250 < Date.now()) {
    // Flush buffer
    c.stdin.write(silence);
    lastTraffic = 0;
  }
}, 25);

function selectSuitable(sortedCalls) {
  // Select a suitable
  for (let i = 0; i < sortedCalls.length; i++) {
    if (
      sortedCalls[i].voiceTime + 1000 > Date.now() &&
      !sortedCalls[i].duplex
    ) {
      sortedCalls[i].play = true;
      break;
    }
    // Don't loop over every call
    if (sortedCalls[i].voiceTime + 1000 < Date.now()) {
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
    c.stdin.write(buffer);
  }
}
