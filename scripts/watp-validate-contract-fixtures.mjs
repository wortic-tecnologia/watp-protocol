#!/usr/bin/env node

import fs from "node:fs";
import path from "node:path";
import process from "node:process";

const repoRoot = process.cwd();
const fixturesDir = path.join(repoRoot, "protocol", "fixtures");
const schemaPath = path.join(repoRoot, "protocol", "watp-message-envelope.schema.json");

const cases = [
  {
    file: "valid-hash-interest.json",
    shouldPass: true,
  },
  {
    file: "valid-hash-concluded-payment-url.json",
    shouldPass: true,
  },
  {
    file: "invalid-hash-interest-missing-buyer-url.json",
    shouldPass: false,
    expectedError: "buyer_url is required for HASH_INTEREST and must be a public HTTPS URL",
  },
  {
    file: "invalid-hash-counter-missing-negotiation-id.json",
    shouldPass: false,
    expectedError: "negotiation_id is required for non-INTEREST messages",
  },
  {
    file: "invalid-hash-concluded-invalid-payment-url.json",
    shouldPass: false,
    expectedError: "payment_url must be a public HTTPS URL",
  },
];

function isObject(value) {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function normalizeNodeUrl(url) {
  if (typeof url !== "string" || url.trim() === "") {
    return null;
  }

  let parsed;
  try {
    parsed = new URL(url);
  } catch {
    return null;
  }

  const host = parsed.hostname.toLowerCase();
  if (parsed.protocol !== "https:" || host === "") {
    return null;
  }

  if (parsed.username || parsed.password || parsed.search || parsed.hash) {
    return null;
  }

  if (host === "localhost" || host.endsWith(".local") || host.endsWith(".internal")) {
    return null;
  }

  if (/^\d+\.\d+\.\d+\.\d+$/.test(host)) {
    const octets = host.split(".").map(Number);
    if (
      octets[0] === 10 ||
      octets[0] === 127 ||
      (octets[0] === 172 && octets[1] >= 16 && octets[1] <= 31) ||
      (octets[0] === 192 && octets[1] === 168) ||
      (octets[0] === 169 && octets[1] === 254)
    ) {
      return null;
    }
  }

  const port = parsed.port ? `:${parsed.port}` : "";
  const pathname = parsed.pathname.replace(/\/$/, "");
  return `${parsed.protocol}//${host}${port}${pathname}`;
}

function normalizePublicUrl(url) {
  if (typeof url !== "string" || url.trim() === "") {
    return null;
  }

  let parsed;
  try {
    parsed = new URL(url);
  } catch {
    return null;
  }

  const host = parsed.hostname.toLowerCase();
  if (parsed.protocol !== "https:" || host === "") {
    return null;
  }

  if (parsed.username || parsed.password || parsed.hash) {
    return null;
  }

  if (host === "localhost" || host.endsWith(".local") || host.endsWith(".internal")) {
    return null;
  }

  if (/^\d+\.\d+\.\d+\.\d+$/.test(host)) {
    const octets = host.split(".").map(Number);
    if (
      octets[0] === 10 ||
      octets[0] === 127 ||
      (octets[0] === 172 && octets[1] >= 16 && octets[1] <= 31) ||
      (octets[0] === 192 && octets[1] === 168) ||
      (octets[0] === 169 && octets[1] === 254)
    ) {
      return null;
    }
  }

  const port = parsed.port ? `:${parsed.port}` : "";
  return `${parsed.protocol}//${host}${port}${parsed.pathname}${parsed.search}`;
}

function validatePayload(type, payload) {
  if (type === "HASH_CANCELLED") {
    return null;
  }

  const sku = typeof payload.sku === "string" ? payload.sku.trim() : "";
  const productName =
    typeof payload.item === "string"
      ? payload.item.trim()
      : typeof payload.product_name === "string"
        ? payload.product_name.trim()
        : "";
  const qty = Number.parseInt(String(payload.qty ?? payload.quantity ?? 0), 10);
  const price = Number.parseFloat(String(payload.unit_price ?? payload.price ?? 0));

  if (sku === "" && productName === "") {
    return "sku or product_name is required";
  }

  if (!Number.isFinite(qty) || qty <= 0) {
    return "qty must be greater than zero";
  }

  if (!Number.isFinite(price) || price < 0) {
    return "unit_price must be zero or greater";
  }

  if (type === "HASH_INTEREST" && normalizeNodeUrl(String(payload.buyer_url ?? "")) === null) {
    return "buyer_url is required for HASH_INTEREST and must be a public HTTPS URL";
  }

  if (payload.payment_url && normalizePublicUrl(String(payload.payment_url)) === null) {
    return "payment_url must be a public HTTPS URL";
  }

  return null;
}

import crypto from "node:crypto";

function validateEnvelope(message) {
  if (!isObject(message) || typeof message.type !== "string" || message.type.trim() === "") {
    return "Invalid message: type is required";
  }

  const validTypes = new Set([
    "HASH_INTEREST",
    "HASH_PROPOSAL",
    "HASH_COUNTER",
    "HASH_CONCLUDED",
    "HASH_CANCELLED",
    "HASH_WAITING_HUMAN",
  ]);
  if (!validTypes.has(message.type)) {
    return `Invalid message type: ${message.type}`;
  }

  if (!isObject(message.payload)) {
    return "payload must be an object";
  }

  if (typeof message.id !== "string" || message.id.trim() === "") {
    return "id is required";
  }

  if (typeof message.sender !== "string" || message.sender.trim() === "") {
    return "sender is required";
  }

  if (typeof message.hash !== "string" || !/^[a-f0-9]{64}$/.test(message.hash)) {
    return "hash and timestamp are required";
  }

  if (typeof message.timestamp !== "string" || message.timestamp.trim() === "") {
    return "hash and timestamp are required";
  }

  const payloadError = validatePayload(message.type, message.payload);
  if (payloadError !== null) {
    return payloadError;
  }

  if (message.type !== "HASH_INTEREST") {
    if (typeof message.negotiation_id !== "string" || message.negotiation_id.trim() === "") {
      return "negotiation_id is required for non-INTEREST messages";
    }
  }

  if (message.previousHash !== undefined && message.previousHash !== null && !/^[a-f0-9]{64}$/.test(String(message.previousHash))) {
    return "previousHash must be a 64-char hex string when present";
  }

  if (message.prev_hash !== undefined && message.prev_hash !== null && !/^[a-f0-9]{64}$/.test(String(message.prev_hash))) {
    return "prev_hash must be a 64-char hex string when present";
  }

  const expectedHash = crypto
    .createHash("sha256")
    .update(
      JSON.stringify({
        type: message.type,
        payload: message.payload,
        previousHash: message.previousHash ?? message.prev_hash ?? null,
        timestamp: message.timestamp,
      }),
      "utf8"
    )
    .digest("hex");

  if (expectedHash !== message.hash) {
    return "Invalid hash";
  }

  return null;
}

let failures = 0;

for (const testCase of cases) {
  const fixturePath = path.join(fixturesDir, testCase.file);
  const raw = fs.readFileSync(fixturePath, "utf8");
  const message = JSON.parse(raw);
  const error = validateEnvelope(message);
  const passed = error === null;

  if (testCase.shouldPass && !passed) {
    failures += 1;
    console.error(`FAIL ${testCase.file}: expected success, got "${error}"`);
    continue;
  }

  if (!testCase.shouldPass) {
    if (passed) {
      failures += 1;
      console.error(`FAIL ${testCase.file}: expected failure, got success`);
      continue;
    }
    if (testCase.expectedError && error !== testCase.expectedError) {
      failures += 1;
      console.error(`FAIL ${testCase.file}: expected "${testCase.expectedError}", got "${error}"`);
      continue;
    }
  }

  console.log(`OK ${testCase.file}`);
}

const schema = JSON.parse(fs.readFileSync(schemaPath, "utf8"));
if (!Array.isArray(schema.examples)) {
  failures += 1;
  console.error("FAIL watp-message-envelope.schema.json: examples must be an array");
} else {
  schema.examples.forEach((example, index) => {
    const error = validateEnvelope(example);
    if (error !== null) {
      failures += 1;
      console.error(`FAIL watp-message-envelope.schema.json example ${index + 1}: ${error}`);
      return;
    }

    console.log(`OK watp-message-envelope.schema.json example ${index + 1}`);
  });
}

if (failures > 0) {
  process.exit(1);
}

console.log("Contract fixtures: OK");
