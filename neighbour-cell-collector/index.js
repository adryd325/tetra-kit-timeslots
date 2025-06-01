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

let celldb = {};
try {
  JSON.parse(fs.readFileSync("./cells.json", "utf-8"));
} catch (e) {}

let currLa;
let currLaDb = {};

function loadCurrLaDb() {
  try {
    JSON.parse(fs.readFileSync("./cells-" + currLa + ".json", "utf-8"));
  } catch (e) {
    currLaDb = {};
  }
}

function onMessage(msg, remoteInfo) {
  if (msg.service == "MLE" && msg.pdu == "D-NWRK-BROADCAST") {
    if (msg.cell_la && msg.cell_la != currLa) {
      loadCurrLaDb();
      currLa = msg.cell_la;
    }
    for (let index = 0; index < msg["number of neighbour cells"]; index++) {
      const cell_arr = msg["cell " + index];
      let cell = {};
      cell_arr.forEach((kvp) => {
        cell[Object.keys(kvp)[0]] = Object.values(kvp)[0];
      });
      // console.log(cell)
      if (!celldb[cell.LA]) {
        celldb[cell.LA] = {};
      }
      if (!currLaDb[cell.LA]) {
        currLaDb[cell.LA] = {};
      }
      celldb[cell.LA] = Object.assign(cell, celldb[cell.LA]);
      currLaDb[cell.LA] = Object.assign(cell, currLaDb[cell.LA]);

      let date = new Date().toLocaleDateString("en-CA", { dateStyle: "short" });

      fs.existsSync(date) || fs.mkdirSync(date);

      fs.writeFileSync("./cells.json", JSON.stringify(celldb, null, 2));
      fs.writeFileSync(
        "./" + date + "/cells.json",
        JSON.stringify(celldb, null, 2)
      );
      if (currLa) {
        fs.writeFileSync(
          "./cells-" + currLa + ".json",
          JSON.stringify(currLaDb, null, 2)
        );
        fs.writeFileSync(
          "./" + date + "/cells-" + currLa + ".json",
          JSON.stringify(currLaDb, null, 2)
        );
      }
    }
  }
}
