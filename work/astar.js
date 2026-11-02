// Grid A* router for one connection, then commits it (vias first, tracks after, all locked).
// inputs.job = {net, w, start:[x,y], startLayers:[0,1], goal:["J3.1",...], viaCost, layers:[0,1], box:[x0,y0,x1,y1], dry}
// remote_v2 copy: layer 0 = TOP, 1 = BOTTOM. Clearance 0.2 mm; vias 0.62/0.31; board-edge box + HC-12 antenna keepout (both layers).
const job = JSON.parse(inputs.job);
const mm = v => v * 0.0254, mil = v => v / 0.0254;
const NET = job.net, G = 0.1, [X0, Y0, X1, Y1] = job.box || [-25, -28, 25, 28];
const NX = Math.round((X1 - X0) / G) + 1, NY = Math.round((Y1 - Y0) / G) + 1, NN = NX * NY;
const D = [new Float32Array(NN).fill(99), new Float32Array(NN).fill(99)], GOAL = [new Uint8Array(NN), new Uint8Array(NN)];
const NOVIA = new Uint8Array(NN);   // own-net SMD pads: no via-in-pad
const R = 1.2;
const mark = (L, x0, y0, x1, y1, f, goal) => {
  const i0 = Math.max(0, Math.floor((x0 - X0) / G)), i1 = Math.min(NX - 1, Math.ceil((x1 - X0) / G)), j0 = Math.max(0, Math.floor((y0 - Y0) / G)), j1 = Math.min(NY - 1, Math.ceil((y1 - Y0) / G));
  for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++) { const d = f(X0 + i * G, Y0 + j * G); for (const l of L) { if (goal) { if (d <= -0.05) GOAL[l][j * NX + i] = 1; } else if (d < D[l][j * NX + i]) D[l][j * NX + i] = d; } }
};
const segD = (px, py, x1, y1, x2, y2) => { const dx = x2 - x1, dy = y2 - y1, L = dx * dx + dy * dy; let t = L ? ((px - x1) * dx + (py - y1) * dy) / L : 0; t = Math.max(0, Math.min(1, t)); return Math.hypot(px - x1 - t * dx, py - y1 - t * dy); };
for (const l of await eda.pcb_PrimitiveLine.getAll()) {
  const ly = l.getState_Layer(); if (![1, 2].includes(ly) || l.getState_Net() === NET) continue;
  const x1 = mm(l.getState_StartX()), y1 = mm(l.getState_StartY()), x2 = mm(l.getState_EndX()), y2 = mm(l.getState_EndY()), w = mm(l.getState_LineWidth()) / 2;
  mark([ly - 1], Math.min(x1, x2) - w - R, Math.min(y1, y2) - w - R, Math.max(x1, x2) + w + R, Math.max(y1, y2) + w + R, (x, y) => segD(x, y, x1, y1, x2, y2) - w);
}
const holes = [];
for (const v of await eda.pcb_PrimitiveVia.getAll()) {
  const x = mm(v.getState_X()), y = mm(v.getState_Y()), r = mm(v.getState_Diameter()) / 2;
  holes.push([x, y]); if (v.getState_Net() === NET) continue;
  mark([0, 1], x - r - R, y - r - R, x + r + R, y + r + R, (px, py) => Math.hypot(px - x, py - y) - r);
}
const padObs = [];
for (const c of await eda.pcb_PrimitiveComponent.getAll()) for (const p of await eda.pcb_PrimitiveComponent.getAllPinsByPrimitiveId(c.getState_PrimitiveId())) {
  const s = p.getState_Pad(), ly = p.getState_Layer(), x = mm(p.getState_X()), y = mm(p.getState_Y()), rot = -(p.getState_Rotation() || 0) * Math.PI / 180;
  const L = ly === 1 ? [0] : ly === 2 ? [1] : [0, 1];
  let hw = mm(s[1] || 1.6) / 2, hh = mm(s[2] || s[1] || 1.6) / 2; if (s[0] === "POLYGON") hw = hh = 0.8;
  const f = (px, py) => { const dx = px - x, dy = py - y, u = Math.abs(dx * Math.cos(rot) - dy * Math.sin(rot)), v = Math.abs(dx * Math.sin(rot) + dy * Math.cos(rot)); if (s[0] === "ELLIPSE") return Math.hypot(u, v) - hw; const ox = u - hw, oy = v - hh; return ox > 0 || oy > 0 ? Math.hypot(Math.max(ox, 0), Math.max(oy, 0)) : Math.max(ox, oy); };
  const ext = Math.max(hw, hh) + R, key = c.getState_Designator() + "." + p.getState_PadNumber();
  if (job.goal.includes(key)) mark(L, x - ext, y - ext, x + ext, y + ext, f, true);
  if (p.getState_Net() === NET && ly !== 12) { const i0 = Math.max(0, Math.floor((x - ext - X0) / G)), i1 = Math.min(NX - 1, Math.ceil((x + ext - X0) / G)), j0 = Math.max(0, Math.floor((y - ext - Y0) / G)), j1 = Math.min(NY - 1, Math.ceil((y + ext - Y0) / G)); for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++) if (f(X0 + i * G, Y0 + j * G) < 0.46) NOVIA[j * NX + i] = 1; }
  else if (p.getState_Net() !== NET) mark(L, x - ext, y - ext, x + ext, y + ext, f);
  if (ly === 12) holes.push([x, y]);
}
mark([0, 1], X0, Y0, X1, Y1, (x, y) => Math.min(x - 0.03, 36.03 - x, y - 5.3, 68.2 - y) - 0.3);
mark([0, 1], 10.9, -1.6, 25.2, 4.4, (x, y) => Math.max(10.92 - x, x - 25.15, -1.52 - y, y - 4.32));
const KO = () => false;
const TW = job.w / 2 + 0.2, VR = 0.31 + 0.2, VCOST = job.viaCost ?? 40, LAY = job.layers || [0, 1];
const xy = k => [X0 + (k % NX) * G, Y0 + Math.floor(k / NX) * G];
const okT = (l, k) => LAY.includes(l) && D[l][k] >= TW && !(l === 0 && KO(...xy(k)));
const okV = k => { const [x, y] = xy(k); return LAY.length > 1 && !NOVIA[k] && D[0][k] >= VR && D[1][k] >= VR && !KO(x, y) && holes.every(([hx, hy]) => Math.hypot(x - hx, y - hy) >= 0.75); };
let gx = 0, gy = 0, gn = 0; for (const l of [0, 1]) for (let k = 0; k < NN; k++) if (GOAL[l][k]) { const [x, y] = xy(k); gx += x; gy += y; gn++; }
if (!gn) return { error: "no goal cells for " + job.goal };
gx /= gn; gy /= gn;
const S = Math.round((job.start[1] - Y0) / G) * NX + Math.round((job.start[0] - X0) / G);
const gS = new Float32Array(NN * 2).fill(1e9), prev = new Int32Array(NN * 2).fill(-1), heap = [];
const push = (f, s) => { heap.push([f, s]); let i = heap.length - 1; while (i > 0) { const p = (i - 1) >> 1; if (heap[p][0] <= heap[i][0]) break; [heap[p], heap[i]] = [heap[i], heap[p]]; i = p; } };
const pop = () => { const t = heap[0], e = heap.pop(); if (heap.length) { heap[0] = e; let i = 0; for (;;) { const a = 2 * i + 1, b = a + 1; let m = i; if (a < heap.length && heap[a][0] < heap[m][0]) m = a; if (b < heap.length && heap[b][0] < heap[m][0]) m = b; if (m === i) break; [heap[m], heap[i]] = [heap[i], heap[m]]; i = m; } } return t; };
const h = k => { const [x, y] = xy(k); return Math.hypot(x - gx, y - gy) / G * 0.9; };
for (const l of job.startLayers || [0, 1]) { gS[l * NN + S] = 0; push(h(S), l * NN + S); }
const dirs = [[1, 0, 1], [-1, 0, 1], [0, 1, 1], [0, -1, 1], [1, 1, 1.414], [1, -1, 1.414], [-1, 1, 1.414], [-1, -1, 1.414]];
let found = -1, iter = 0;
while (heap.length && iter < 5e6) {
  iter++;
  const [f, s] = pop(); const l = s >= NN ? 1 : 0, k = s - l * NN, g = gS[s];
  if (f - h(k) > g + 1e-3) continue;
  if (GOAL[l][k] && LAY.includes(l)) { found = s; break; }
  const i = k % NX, j = Math.floor(k / NX);
  for (const [di, dj, c] of dirs) {
    const ni = i + di, nj = j + dj; if (ni < 0 || nj < 0 || ni >= NX || nj >= NY) continue;
    const nk = nj * NX + ni; if (!(okT(l, nk) || (GOAL[l][nk] && LAY.includes(l)))) continue;
    const ns = l * NN + nk, ng = g + c + (prev[s] >= 0 && (s - prev[s]) !== (ns - s) ? 0.5 : 0);
    if (ng < gS[ns]) { gS[ns] = ng; prev[ns] = s; push(ng + h(nk), ns); }
  }
  const os = (1 - l) * NN + k;
  if (okT(1 - l, k) && okV(k)) { const ng = g + VCOST; if (ng < gS[os]) { gS[os] = ng; prev[os] = s; push(ng + h(k), os); } }
}
if (found < 0) return { found: false, iter, startD: [D[0][S], D[1][S]] };
const pts = []; for (let s = found; s >= 0; s = prev[s]) { const l = s >= NN ? 1 : 0, k = s - l * NN; const [x, y] = xy(k); pts.push([+x.toFixed(2), +y.toFixed(2), l]); }
pts.reverse();
const P = [pts[0]];
for (let i = 1; i < pts.length - 1; i++) { const a = P[P.length - 1], b = pts[i], c = pts[i + 1]; if (b[2] !== a[2] || c[2] !== b[2]) { P.push(b); continue; } if (Math.abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) > 1e-6) P.push(b); }
P.push(pts[pts.length - 1]);
if (job.dry) return { found: true, iter, path: P };
P[0] = [job.start[0], job.start[1], P[0][2]];
const vias = [];
for (let i = 1; i < P.length; i++) if (P[i][2] !== P[i - 1][2]) vias.push([P[i][0], P[i][1]]);
for (const [x, y] of vias) { const v = await eda.pcb_PrimitiveVia.create(NET, mil(x), mil(y), mil(0.31), mil(0.62)); await eda.pcb_PrimitiveVia.modify(v.getState_PrimitiveId(), { primitiveLock: true }); }
let nt = 0;
for (let i = 1; i < P.length; i++) if (P[i][2] === P[i - 1][2]) { await eda.pcb_PrimitiveLine.create(NET, P[i][2] + 1, mil(P[i - 1][0]), mil(P[i - 1][1]), mil(P[i][0]), mil(P[i][1]), mil(job.w), true); nt++; }
await eda.pcb_Document.save();
return { found: true, iter, tracks: nt, vias: vias.length, path: P };
