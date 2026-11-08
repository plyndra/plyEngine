// plyEngine companion controller.
//
// Speaks the versioned JSON protocol over a WebSocket: join with a room code,
// then stream controller state. All network work happens here; the host's
// simulation only ever sees drained input at fixed-step boundaries.

const PROTOCOL_VERSION = 1;

const BUTTON_A = 1 << 0;
const BUTTON_B = 1 << 1;

interface Envelope {
  v: number;
  type: string;
  seq: number;
  payload: Record<string, unknown>;
}

interface ControllerState {
  x: number;
  y: number;
  buttons: number;
}

const state: ControllerState = { x: 0, y: 0, buttons: 0 };
let socket: WebSocket | null = null;
let seq = 0;
let joined = false;
let reconnectTimer: number | null = null;
let roomCode = "";
let playerName = "";
let inputDirty = false;

// --- DOM -------------------------------------------------------------------

const joinPanel = document.getElementById("join") as HTMLElement;
const padPanel = document.getElementById("pad") as HTMLElement;
const roomInput = document.getElementById("room-input") as HTMLInputElement;
const nameInput = document.getElementById("name-input") as HTMLInputElement;
const joinButton = document.getElementById("join-button") as HTMLButtonElement;
const joinError = document.getElementById("join-error") as HTMLElement;
const playerLabel = document.getElementById("player-label") as HTMLElement;
const linkState = document.getElementById("link-state") as HTMLElement;
const btnA = document.getElementById("btn-a") as HTMLButtonElement;
const btnB = document.getElementById("btn-b") as HTMLButtonElement;

// Pre-fill the room code from ?room=ABCD so a printed link just works.
const params = new URLSearchParams(window.location.search);
const prefill = params.get("room");
if (prefill) {
  roomInput.value = prefill.toUpperCase();
}
nameInput.value = localStorage.getItem("plyEngine.playerName") ?? "";

// --- protocol --------------------------------------------------------------

function send(type: string, payload: Record<string, unknown> = {}): void {
  if (!socket || socket.readyState !== WebSocket.OPEN) {
    return;
  }
  const envelope: Envelope = { v: PROTOCOL_VERSION, type, seq: ++seq, payload };
  socket.send(JSON.stringify(envelope));
}

function websocketUrl(): string {
  const scheme = window.location.protocol === "https:" ? "wss" : "ws";
  return `${scheme}://${window.location.host}/ws`;
}

function connect(): void {
  socket = new WebSocket(websocketUrl());

  socket.addEventListener("open", () => {
    linkState.classList.add("online");
    send("join", { room: roomCode, name: playerName });
  });

  socket.addEventListener("message", (event) => {
    let message: Envelope;
    try {
      message = JSON.parse(String(event.data)) as Envelope;
    } catch {
      return;
    }

    if (message.type === "joined") {
      joined = true;
      joinPanel.classList.add("hidden");
      padPanel.classList.remove("hidden");
      const name = String(message.payload["name"] ?? playerName);
      playerLabel.textContent = name;
      joinError.textContent = "";
      return;
    }

    if (message.type === "rejected") {
      joined = false;
      socket?.close();
      joinPanel.classList.remove("hidden");
      padPanel.classList.add("hidden");
      joinError.textContent = String(message.payload["reason"] ?? "could not join");
      return;
    }
  });

  socket.addEventListener("close", () => {
    linkState.classList.remove("online");
    joined = false;
    // Reconnect with backoff while the player still wants to be in the game.
    if (reconnectTimer === null && padPanel.classList.contains("hidden") === false) {
      reconnectTimer = window.setTimeout(() => {
        reconnectTimer = null;
        connect();
      }, 1500);
    }
  });

  socket.addEventListener("error", () => {
    linkState.classList.remove("online");
  });
}

// --- input -----------------------------------------------------------------

function recomputeAxes(): void {
  let x = 0;
  let y = 0;
  for (const button of document.querySelectorAll<HTMLButtonElement>(".dir")) {
    if (!button.classList.contains("active")) {
      continue;
    }
    switch (button.dataset["dir"]) {
      case "left":
        x -= 1;
        break;
      case "right":
        x += 1;
        break;
      case "up":
        y += 1;
        break;
      case "down":
        y -= 1;
        break;
    }
  }
  state.x = x;
  state.y = y;
  inputDirty = true;
}

function bindHold(element: HTMLButtonElement, onDown: () => void, onUp: () => void): void {
  const down = (event: Event) => {
    event.preventDefault();
    element.classList.add("active");
    onDown();
  };
  const up = (event: Event) => {
    event.preventDefault();
    element.classList.remove("active");
    onUp();
  };

  element.addEventListener("pointerdown", down);
  element.addEventListener("pointerup", up);
  element.addEventListener("pointercancel", up);
  element.addEventListener("pointerleave", up);
  element.addEventListener("contextmenu", (event) => event.preventDefault());
}

for (const button of document.querySelectorAll<HTMLButtonElement>(".dir")) {
  bindHold(button, recomputeAxes, recomputeAxes);
}

bindHold(
  btnA,
  () => {
    state.buttons |= BUTTON_A;
    inputDirty = true;
  },
  () => {
    state.buttons &= ~BUTTON_A;
    inputDirty = true;
  },
);

bindHold(
  btnB,
  () => {
    state.buttons |= BUTTON_B;
    inputDirty = true;
  },
  () => {
    state.buttons &= ~BUTTON_B;
    inputDirty = true;
  },
);

// Send at a fixed cadence rather than per touch event, so a burst of pointer
// moves cannot flood the socket. Slightly faster than the sim's 60 Hz.
const SEND_INTERVAL_MS = 33;
window.setInterval(() => {
  if (!joined || !inputDirty) {
    return;
  }
  inputDirty = false;
  send("input", { x: state.x, y: state.y, buttons: state.buttons });
}, SEND_INTERVAL_MS);

// --- join ------------------------------------------------------------------

joinButton.addEventListener("click", () => {
  roomCode = roomInput.value.trim().toUpperCase();
  playerName = nameInput.value.trim();
  if (!roomCode || !playerName) {
    joinError.textContent = "Enter a room code and a name";
    return;
  }
  localStorage.setItem("plyEngine.playerName", playerName);
  joinError.textContent = "";
  connect();
});

roomInput.addEventListener("keydown", (event) => {
  if (event.key === "Enter") {
    joinButton.click();
  }
});
