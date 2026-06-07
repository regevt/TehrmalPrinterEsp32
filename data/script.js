document.addEventListener("DOMContentLoaded", () => {
  const textForm = document.getElementById("textForm");
  const fileInput = document.getElementById("fileInput");
  const canvas = document.getElementById("previewCanvas");
  const ctx = canvas.getContext("2d");
  const printImgBtn = document.getElementById("printImgBtn");
  const printQrBtn = document.getElementById("printQrBtn");
  const qrInput = document.getElementById("qrValue");
  const thresholdInput = document.getElementById("threshold");
  const tzInfo = document.getElementById("tz-info");

  const MAX_ROW_BYTES = 72;
  const MAX_BITS = MAX_ROW_BYTES * 8; // 576 pixels

  let currentImage = new Image();
  let activeBinaryPayload = null;

  fetch("/wifi-status")
    .then((r) => r.json())
    .then((data) => {
      tzInfo.textContent = `Timezone Context: ${data.timezone || "N/A"} | IP: ${data.ip}`;
    })
    .catch((err) => console.error("Error fetching wifi status:", err));

  reloadHistoryQueue();

  function reloadHistoryQueue() {
    fetch("/history-data")
      .then((r) => r.json())
      .then((items) => {
        const container = document.getElementById("historyLog");
        if (!items || items.length === 0) {
          container.innerHTML = "<em>No records present in history buffer.</em>";
          return;
        }
        container.innerHTML = items
          .map(
            (item) => `
                        <div class="history-item">
                            <div>
                                <div class="message-text">${item.text}</div>
                                <div class="timestamp">Printed at ${item.time}</div>
                            </div>
                            <button onclick="reprintRecord(${item.id})">Seal</button>
                        </div>
                    `,
          )
          .join("");
      })
      .catch((err) => console.error("Error loading history:", err));
  }

  window.reprintRecord = function (id) {
    fetch(`/print?index=${id}`).then((r) => {
      if (r.ok) reloadHistoryQueue();
    });
  };

  textForm.addEventListener("submit", async (e) => {
    e.preventDefault();
    const input = document.getElementById("inputValue");
    const formData = new URLSearchParams();
    formData.append("inputValue", input.value);

    const response = await fetch("/submit", { method: "POST", body: formData });
    if (response.ok) {
      input.value = "";
      reloadHistoryQueue();
    }
  });

  printQrBtn.addEventListener("click", async () => {
    const text = qrInput.value.trim();
    if (!text) return;

    printQrBtn.disabled = true;
    printQrBtn.textContent = "Generating QR...";

    const qr = new QRious({ value: text, size: 300, level: "L" });

    currentImage = new Image();
    currentImage.src = qr.toDataURL();
    currentImage.onload = () => {
      const payload = generateBinaryPayload();
      if (payload) {
        sendBinaryToPrinter(payload, printQrBtn, "Print QR Code");
        qrInput.value = "";
      }
    };
  });

  fileInput.addEventListener("change", (e) => {
    const file = e.target.files[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = (evt) => {
      currentImage = new Image();
      currentImage.src = evt.target.result;
      currentImage.onload = () => {
        activeBinaryPayload = generateBinaryPayload();
        printImgBtn.disabled = false;
      };
    };
    reader.readAsDataURL(file);
  });

  thresholdInput.addEventListener("input", () => {
    if (currentImage.src) {
      activeBinaryPayload = generateBinaryPayload();
    }
  });

  function generateBinaryPayload() {
    if (!currentImage.src) return null;

    const factor = MAX_BITS / currentImage.width;
    const targetWidth = Math.max(1, Math.floor(currentImage.width * factor));
    const targetHeight = Math.max(1, Math.floor(currentImage.height * factor));

    canvas.width = MAX_BITS;
    canvas.height = targetHeight;

    ctx.fillStyle = "#FFFFFF";
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    ctx.drawImage(currentImage, 0, 0, targetWidth, targetHeight);

    const imgData = ctx.getImageData(0, 0, canvas.width, canvas.height);
    const data = imgData.data;

    const currentThreshold = parseInt(thresholdInput.value, 10);

    const grayBuf = new Float32Array(MAX_BITS * targetHeight);
    for (let i = 0; i < data.length; i += 4) {
      grayBuf[i / 4] = 0.299 * data[i] + 0.587 * data[i + 1] + 0.114 * data[i + 2];
    }

    const binaryPayload = new Uint8Array(targetHeight * MAX_ROW_BYTES);

    for (let y = 0; y < targetHeight; y++) {
      for (let x = 0; x < MAX_BITS; x++) {
        const oldIdx = y * MAX_BITS + x;
        const oldPixel = grayBuf[oldIdx];

        const newPixel = oldPixel < currentThreshold ? 0 : 255;
        const err = oldPixel - newPixel;

        if (x + 1 < MAX_BITS) grayBuf[oldIdx + 1] += (err * 7) / 16;
        if (y + 1 < targetHeight) {
          if (x > 0) grayBuf[oldIdx + MAX_BITS - 1] += (err * 3) / 16;
          if (true) grayBuf[oldIdx + MAX_BITS] += (err * 5) / 16;
          if (x + 1 < MAX_BITS) grayBuf[oldIdx + MAX_BITS + 1] += (err * 1) / 16;
        }

        const outDataIdx = oldIdx * 4;
        data[outDataIdx] = data[outDataIdx + 1] = data[outDataIdx + 2] = newPixel;

        if (newPixel === 0) {
          const byteIdx = y * MAX_ROW_BYTES + Math.floor(x / 8);
          const bitIdx = 7 - (x % 8);
          binaryPayload[byteIdx] |= 1 << bitIdx;
        }
      }
    }

    ctx.putImageData(imgData, 0, 0);

    return binaryPayload;
  }

  printImgBtn.addEventListener("click", () => {
    if (activeBinaryPayload) {
      sendBinaryToPrinter(activeBinaryPayload, printImgBtn, "Print Graphic Image");
    }
  });

  function sendBinaryToPrinter(payload, buttonEl, originalText) {
    buttonEl.disabled = true;
    buttonEl.textContent = "Streaming...";

    const ws = new WebSocket(`ws://${window.location.hostname}:81`);
    ws.binaryType = "arraybuffer";

    ws.onopen = async () => {
      try {
        console.log("🚀 Connection secured. Initializing stream tracking markers...");
        ws.send("START_PRINT");

        await new Promise((r) => setTimeout(r, 100));

        const totalBytes = payload.byteLength;
        const chunkSize = 72;
        let offset = 0;

        console.log(`📦 Slicing ${totalBytes} total bytes into chunks of ${chunkSize}...`);

        while (offset < totalBytes) {
          const chunk = payload.slice(offset, offset + chunkSize);
          ws.send(chunk);

          offset += chunkSize;
          await new Promise((r) => setTimeout(r, 2));
        }

        await new Promise((r) => setTimeout(r, 100));

        console.log("🏁 All chunks transmitted successfully. Sending termination tag.");
        ws.send("END_PRINT");
      } catch (err) {
        console.error("❌ Exception inside active stream loop:", err);
      }
    };

    ws.onmessage = (evt) => {
      console.log("📩 Message received from ESP32:", evt.data);
      if (evt.data === "PRINT_SUCCESS") {
        alert("✨ Document printed successfully via WebSocket chunks!");
        ws.close();
      }
    };

    ws.onerror = (err) => {
      console.error("❌ WebSocket Error Tracked:", err);
    };

    ws.onclose = (evt) => {
      console.log(`🔒 Socket closed cleanly. Status Code: ${evt.code}`);
      buttonEl.disabled = false;
      buttonEl.textContent = originalText;
    };
  }
});
