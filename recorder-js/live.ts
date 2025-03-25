import * as fs from "fs";
import { mkfifoSync } from "mkfifo";
import { Call, calls } from "./call.ts";
import { Readable } from "stream";
import child_process from "child_process";

// I excuse myself using synchronous operations since this is early in execution and only runs once
// child_process.spawnSync("rm", ["./fifo"]);
// child_process.spawnSync("mkfifo", ["./fifo"]);
let silence = fs.readFileSync("./silence.acelp")
let c = child_process.spawn("./play");
// const fd = fs.openSync("./fifo", "w");
c.stderr.pipe(process.stdout);


let lastTraffic = 0;

function sortCallsByActivity(a, b) {
  if (a.voiceTime < b.voiceTime) {
    return 1;
  } else if (a.voiceTime > b.voiceTime) {
    return -1;
  }
  return 0;
}

const closeInterval = setInterval(() => {
  if (lastTraffic != 0 && lastTraffic + 100 < Date.now()) {
    c.stdin.write(silence);
    c.stdin.end();
    // c.on("close", () => { console.log("closed")})

    lastTraffic = 0;
    c = child_process.spawn("./play");
  }
}, 25)

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
  } else if (nowPlaying && nowPlaying.voiceTime + 1000 < Date.now()) {
    // select new call to play
    nowPlaying.play = false;
    selectSuitable(sortedCalls);
  }
  // No change
}

export function submitPlaying(call: Call, buffer: Buffer, force?: boolean) {
  selectPlaying();
  if (force || call.play) {
    lastTraffic = Date.now()
    console.log(`\x1b[31mNOW PLAYING\x1b[0m tg:${call.gssi} cid:${call.cid}`)
    c.stdin.cork()
    c.stdin.write(buffer)
    c.stdin.uncork()
  }
}
