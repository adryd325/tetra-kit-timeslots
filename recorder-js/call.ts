import * as child_process from "child_process";
import * as fs from "fs";
import type { FileHandle } from "fs/promises";
import * as path from "path";

const MATCH_ONLY_USAGE = false;
const UNPROCESSED_FOLDER = "unprocessed";
const OUT_FOLDER = "out";

export const calls: Set<Call> = new Set();

export function getCallByCid(cid): Call {
  let call;
  for (call of calls) {
    if (call.cid === cid) {
      return call;
    }
  }
  // Create if doesnt exist
  call = new Call(cid);
  calls.add(call);
  return call;
}

export function getCallByDownlinkInfo(usage, carrier, timeslot): Call | null {
  let call;
  for (call of calls) {
    if (call.usage === usage) {
      // Matching timeslots miiiight be finnicky, lets do our best
      if (
        MATCH_ONLY_USAGE ||
        call.carrier == carrier && call.timeslot == timeslot
      ) {
        return call;
      }
    }
  }
  return null;
}

export function releaseCallByCid(cid) {
  for (let call of calls) {
    if (call.cid === cid) {
      if (call.fileName != null) {
        call.finish();
      }
      calls.delete(call);
    }
  }
}

const cleanupInteval = setInterval((): void => {
  for (let call of calls) {
    // Clear calls that were never recorded
    if (call.createTime + 60000 < Date.now()) {
      if (call.voiceTime == null) {
        calls.delete(call);
      }
    }

    // Flush calls after a voice timeout of 10 seconds
    if (call.voiceTime != null && call.voiceTime + 10000 < Date.now()) {
      call.finish();
    }

    // Expire SSIs after 30 seconds
    let lastHeard;
    for (let i of call.ssis.keys()) {
      lastHeard = call.ssis.get(i);
      if (lastHeard + 30000 < Date.now()) {
        call.ssis.delete(i);
      }
    }

    // Expire Calls
    if (
      (call.voiceTime != null && call.voiceTime + 60000 < Date.now()) ||
      (call.cmceTime != null && call.cmceTime + 60000 < Date.now())
    ) {
      calls.delete(call);
    }
  }
}, 1000);

export class Call {
  cid: number;

  usage: number;
  carrier: number;
  timeslot: number;

  gssi: number | null = null;
  // ssi, lastSeen
  ssis: Map<number, number> = new Map();
  duplex: boolean = false;
  play: boolean = false;

  fileName: string | null = null;
  fd: number | null = null;
  fileCreateTime: Date | null = null;

  createTime: number;
  voiceTime: number | null = null;
  cmceTime: number | null = null;

  constructor(cid) {
    this.createTime = Date.now();
    this.cid = cid;
  }

  private getFilename(date: Date): string {
    let fileName = "";

    // sprintf would be sooo cool right now
    // maybe this is a bit unweildy
    fileName += date.getFullYear();
    fileName += (date.getMonth() + 1).toString().padStart(2, "0");
    fileName += date.getDate().toString().padStart(2, "0");
    fileName += "_";
    fileName += date.getHours().toString().padStart(2, "0");
    fileName += date.getMinutes().toString().padStart(2, "0");
    fileName += date.getSeconds().toString().padStart(2, "0");
    fileName += "_";
    fileName += this.cid.toString().padStart(3, "0");
    fileName += "_";
    fileName += this.usage.toString().padStart(2, "0");
    fileName += "_";
    fileName += this.carrier.toString().padStart(4, "0");
    fileName += "_";
    fileName += this.timeslot.toString().padStart(2, "0");
    fileName += "_";
    fileName += (this.gssi ?? 0).toString().padStart(8, "0");
    let ssiCnt = 0;
    for (let ssi of this.ssis.keys()) {
      if (ssiCnt > 7) break;
      if (ssi == this.gssi) continue;
      fileName += "_";
      fileName += ssi.toString().padStart(8, "0");
    }
    return fileName;
  }

  addSsi(ssi): void {
    this.ssis.set(ssi, Date.now());
  }

  setDownlinkInfo(usage, carrier, timeslot): void {
    if (this.usage != usage || this.carrier != carrier || this.timeslot != timeslot) {
      this.finish();
    }
    this.usage = usage;
    this.carrier = carrier;
    this.timeslot = timeslot;
  }

  submit(buffer: Buffer): Promise<void> {
    // Duplex calls are finnicky and can have both sides on the same cell
    // Causing helicoptering when we try decode. It's best to just ignore
    // Them for now
    if (this.duplex) {
      this.voiceTime = Date.now();
      return;
    }

    // Split calls up into individual speakers
    // TODO: Is this too short? Will a GC cause this to sad?
    if (this.voiceTime != null && this.voiceTime + 10000 < Date.now()) {
      this.finish();
    }

    this.voiceTime = Date.now();

    // Get a filename if we dont have one
    if (!this.fileName) {
      let now = new Date();
      this.fileName = this.getFilename(now);
      this.fileCreateTime = now;
      this.fd = fs.openSync(path.join(UNPROCESSED_FOLDER, this.fileName), "w+");
    }

    // TODO: is this bad?
    fs.writeSync(this.fd, buffer);
  }

  finish(): void {
    let fileName = this.fileName;
    let fd = this.fd;
    let createTime = this.fileCreateTime;
    this.fileName = null;
    this.fd = null;
    this.fileCreateTime = null;

    if (!fd) return;

    // I don't wanna deal with a hundred file handles, processes and pipes
    // There are some things shell scripts are just great for

    // Close then transcode

    fs.close(fd, (err) => {
      if (!err) {
        let ps = child_process.spawn("./transcode", [
          path.join(UNPROCESSED_FOLDER, fileName),
          path.join(OUT_FOLDER, this.getFilename(createTime) + ".mp3"),
        ]);
      }
    });
  }
}
