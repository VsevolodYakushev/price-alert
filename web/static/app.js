'use strict';

// UI Price Alert. Сервер (web/server.cpp) отдаёт JSON, все тексты для пользователя пишутся здесь.

const $ = (selector, root = document) => root.querySelector(selector);

const AUTH_MESSAGES = {
  NeedCode: 'Введите код подтверждения из письма',
  BadCredentials: 'Неверное имя пользователя или пароль',
  UserExists: 'Пользователь с таким именем или email уже существует',
  BadCode: 'Неверный код подтверждения',
  InvalidInput: 'Проверьте введённые данные',
  Error: 'Что-то пошло не так. Попробуйте ещё раз',
};
const OFFLINE = 'Сервер недоступен. Запущен ли PriceAlert.exe?';
const MAX_POINTS = 300;
const MAX_LOG = 200;

const state = {
  user: null,
  pendingUser: '',
  cursor: null,       // номер последнего полученного алерта
  assets: [],
  rows: new Map(),    // id -> <tr>
  history: new Map(), // id -> [{t, p}]
  selectedId: null,
  autoSelect: false,  // после входа выбрать первый актив, чтобы график сразу был виден
  historyLoaded: new Set(),
  editingId: null,
  timer: null,
  polling: false,
};

// ---------- Сеть ----------

async function request(method, url, params) {
  const options = { method, cache: 'no-store' };
  if (params) options.body = new URLSearchParams(params);
  let response;
  try {
    response = await fetch(url, options);
  } catch {
    return { ok: false, status: 0, data: {} };
  }
  let data = {};
  try { data = await response.json(); } catch { /* пустой ответ */ }
  return { ok: response.ok, status: response.status, data };
}

// ---------- Форматирование ----------

const numberFormats = new Map();
function formatNumber(value, digits) {
  let format = numberFormats.get(digits);
  if (!format) {
    format = new Intl.NumberFormat('ru-RU', { minimumFractionDigits: digits, maximumFractionDigits: digits });
    numberFormats.set(digits, format);
  }
  return format.format(value);
}
const formatPrice = (p) => formatNumber(p, Math.abs(p) >= 1 ? 2 : 6);
const formatPercent = (p) => (p > 0 ? '+' : '') + formatNumber(p, 2) + ' %';
const formatTime = (ms) => new Date(ms).toLocaleTimeString('ru-RU');

function setMessage(element, text, kind = 'error') {
  element.textContent = text;
  element.dataset.kind = kind;
}

// ---------- Вход и регистрация ----------

function showPage(id) {
  for (const page of document.querySelectorAll('#auth .page')) page.hidden = page.id !== id;
  const first = $(`#${id} input`);
  if (first) first.focus();
}

function resetAuth() {
  for (const form of document.querySelectorAll('#auth form')) {
    form.reset();
    setMessage($('.msg', form), '');
  }
  state.pendingUser = '';
  showPage('login-form');
}

function askNotificationPermission() {
  if ('Notification' in window && Notification.permission === 'default') {
    Notification.requestPermission().catch(() => {});
  }
}

function handleAuth(response, username, message) {
  if (!response.ok) return setMessage(message, OFFLINE);
  const { result, user } = response.data;

  if (result === 'Ok') {
    if (user) return enterDashboard(user);
    // Регистрация прошла без кода, но сессия не открыта: просим войти.
    const login = $('#login-form');
    showPage('login-form');
    login.elements.username.value = username;
    login.elements.password.focus();
    return setMessage($('.msg', login), 'Аккаунт создан. Войдите с новым паролем', 'info');
  }

  if (result === 'NeedCode') {
    state.pendingUser = username;
    const form = $('#code-form');
    form.reset();
    setMessage($('.msg', form), '');
    $('#code-info').textContent = `Мы отправили код на почту пользователя ${username}. Введите его ниже.`;
    return showPage('code-form');
  }

  setMessage(message, AUTH_MESSAGES[result] || AUTH_MESSAGES.Error);
}

for (const button of document.querySelectorAll('[data-go]')) {
  button.addEventListener('click', () => showPage(button.dataset.go));
}

$('#login-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  const form = event.currentTarget;
  const message = $('.msg', form);
  const username = form.elements.username.value.trim();
  const password = form.elements.password.value;
  if (!username || !password) return setMessage(message, 'Введите имя пользователя и пароль');

  askNotificationPermission();
  const response = await request('POST', '/api/login', { username, password });
  // Пароль стираем только при успехе; при ошибке оставляем и выделяем, чтобы его было легко исправить.
  if (response.ok && (response.data.result === 'Ok' || response.data.result === 'NeedCode')) {
    form.elements.password.value = '';
  } else {
    form.elements.password.focus();
    form.elements.password.select();
  }
  handleAuth(response, username, message);
});

$('#register-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  const form = event.currentTarget;
  const message = $('.msg', form);
  const username = form.elements.username.value.trim();
  const email = form.elements.email.value.trim();
  const password = form.elements.password.value;
  const password2 = form.elements.password2.value;

  let error = '';
  if (!username || !email || !password || !password2) error = 'Заполните все поля';
  else if (!/^[A-Za-z0-9_]{3,32}$/.test(username)) error = 'Имя пользователя: 3–32 символа, латиница, цифры и _';
  else if (!/^[^@\s]+@[^@\s]+\.[^@\s]+$/.test(email)) error = 'Некорректный email';
  else if (password.length < 6) error = 'Пароль должен быть не короче 6 символов';
  else if (password !== password2) error = 'Пароли не совпадают';
  if (error) return setMessage(message, error);

  askNotificationPermission();
  const response = await request('POST', '/api/register', { username, email, password });
  if (response.ok && (response.data.result === 'Ok' || response.data.result === 'NeedCode')) {
    form.elements.password.value = '';
    form.elements.password2.value = '';
  }
  handleAuth(response, username, message);
});

$('#code-form').elements.code.addEventListener('input', (event) => {
  event.target.value = event.target.value.replace(/\D/g, '');
});

$('#code-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  const form = event.currentTarget;
  const message = $('.msg', form);
  const code = form.elements.code.value.trim();
  if (!code) return setMessage(message, 'Введите код из письма');

  const response = await request('POST', '/api/verify', { username: state.pendingUser, code });
  handleAuth(response, state.pendingUser, message);
});

// ---------- Сессия ----------

function enterDashboard(user) {
  state.user = user;
  state.cursor = null;
  state.selectedId = null;
  state.autoSelect = true;
  state.editingId = null;
  $('#auth').hidden = true;
  $('#dashboard').hidden = false;
  $('#username').textContent = user.username;
  setMessage($('#add-form .msg'), '');
  resetAuth();
  poll();
  state.timer = setInterval(poll, 1000);
}

function leaveDashboard() {
  clearInterval(state.timer);
  state.timer = null;
  state.user = null;
  state.assets = [];
  state.history.clear();
  state.historyLoaded.clear();
  state.rows.clear();
  $('#assets').replaceChildren();
  $('#log').replaceChildren();
  $('#status').textContent = '';
  $('#add-form').reset();
  $('#dashboard').hidden = true;
  $('#auth').hidden = false;
  resetAuth();
}

$('#logout').addEventListener('click', async () => {
  await request('POST', '/api/logout');
  leaveDashboard();
});

// Раз в секунду: таблица, история для графика и новые алерты.
async function poll() {
  if (state.polling || !state.user) return;
  state.polling = true;
  try {
    const url = state.cursor === null ? '/api/state' : `/api/state?after=${state.cursor}`;
    const response = await request('GET', url);
    if (!state.user) return;
    if (response.status === 401) return leaveDashboard();
    if (!response.ok) {
      $('#status').textContent = 'Нет связи с сервером…';
      return;
    }
    state.cursor = response.data.cursor;
    state.assets = response.data.assets;
    if (state.autoSelect && state.assets.length) {
      state.autoSelect = false;
      if (state.selectedId === null) state.selectedId = state.assets[0].id;
    }
    appendHistory(state.assets);
    loadHistory(state.selectedId);
    renderTable();
    renderChart();
    for (const alert of response.data.alerts) onAlert(alert);
    $('#status').textContent = `Активов: ${state.assets.length} · обновлено ${formatTime(Date.now())}`;
  } finally {
    state.polling = false;
  }
}

// ---------- Портфель ----------

const addForm = $('#add-form');
const addMessage = $('.msg', addForm);

addForm.elements.symbol.addEventListener('input', (event) => {
  const input = event.target;
  const cleaned = input.value.toUpperCase().replace(/[^A-Z0-9]/g, '');
  if (cleaned !== input.value) input.value = cleaned;
});

addForm.addEventListener('submit', async (event) => {
  event.preventDefault();
  const symbol = addForm.elements.symbol.value.trim().toUpperCase();
  const raw = addForm.elements.price.value.trim().replace(',', '.');
  const target = Number(raw);

  if (!/^[A-Z0-9]{5,20}$/.test(symbol)) {
    addForm.elements.symbol.focus();
    return setMessage(addMessage, 'Тикер как на Binance: 5–20 латинских букв и цифр, например BTCUSDT');
  }
  if (!raw || !Number.isFinite(target) || target <= 0) {
    addForm.elements.price.focus();
    return setMessage(addMessage, 'Укажите целевую цену больше нуля');
  }

  const response = await request('POST', '/api/assets', { symbol, target: raw });
  if (response.status === 409) return setMessage(addMessage, `Не удалось добавить ${symbol}: тикер уже в портфеле или недоступен`);
  if (!response.ok) return setMessage(addMessage, response.status ? 'Ошибка сервера' : OFFLINE);

  setMessage(addMessage, `${symbol} добавлен`, 'info');
  addForm.reset();
  addForm.elements.symbol.focus();
  state.selectedId = response.data.id;
  poll();
});

async function removeSelected() {
  const asset = state.assets.find((a) => a.id === state.selectedId);
  if (!asset) return setMessage(addMessage, 'Выберите актив в таблице');
  const response = await request('POST', '/api/assets/remove', { id: asset.id });
  if (!response.ok) return setMessage(addMessage, response.status ? 'Ошибка сервера' : OFFLINE);
  state.selectedId = null;
  setMessage(addMessage, `${asset.symbol} удалён`, 'info');
  poll();
}

$('#remove').addEventListener('click', removeSelected);
document.addEventListener('keydown', (event) => {
  if (event.key === 'Delete' && state.user && state.editingId === null && !(event.target instanceof HTMLInputElement)) {
    removeSelected();
  }
});

function selectAsset(id) {
  state.selectedId = id;
  renderTable();
  renderChart();
  loadHistory(id);
}

// Один раз на актив подтягиваем накопленную сервером историю и склеиваем с локальными точками.
async function loadHistory(id) {
  if (id === null || state.historyLoaded.has(id)) return;
  state.historyLoaded.add(id);
  const response = await request('GET', `/api/history?id=${id}`);
  if (!response.ok) {
    state.historyLoaded.delete(id);
    return;
  }
  const server = response.data.points.map(([t, p]) => ({ t, p }));
  const local = state.history.get(id) || [];
  const lastServer = server.length ? server[server.length - 1].t : -Infinity;
  const merged = server.concat(local.filter((point) => point.t > lastServer)).slice(-MAX_POINTS);
  state.history.set(id, merged);
  if (id === state.selectedId) renderChart();
}

function createRow(id) {
  const tr = document.createElement('tr');
  for (const cls of ['sym', 'num target', 'num price', 'num', '']) {
    const td = document.createElement('td');
    td.className = cls;
    tr.appendChild(td);
  }
  tr.children[1].title = 'Двойной щелчок — изменить цель';
  tr.addEventListener('click', () => selectAsset(id));
  tr.children[1].addEventListener('dblclick', () => startEdit(id));
  return tr;
}

function updateRow(tr, asset) {
  const [symbol, target, price, delta, enabled] = tr.children;
  const reached = asset.price !== null && asset.price <= asset.target;
  tr.classList.toggle('reached', reached);
  tr.classList.toggle('selected', asset.id === state.selectedId);
  tr.classList.toggle('disabled', !asset.enabled);

  symbol.textContent = asset.symbol;
  if (state.editingId !== asset.id) target.textContent = formatPrice(asset.target);

  const previous = tr.dataset.price === undefined ? null : Number(tr.dataset.price);
  price.textContent = asset.price === null ? '—' : formatPrice(asset.price);
  if (asset.price !== null && previous !== null && asset.price !== previous) {
    price.classList.remove('flash-up', 'flash-down');
    void price.offsetWidth; // перезапуск анимации
    price.classList.add(asset.price > previous ? 'flash-up' : 'flash-down');
  }
  if (asset.price !== null) tr.dataset.price = String(asset.price);

  if (asset.price === null) {
    delta.textContent = '—';
    delta.className = 'num';
  } else {
    delta.textContent = formatPercent((asset.price - asset.target) / asset.target * 100);
    delta.className = 'num ' + (reached ? 'down' : 'up');
  }
  enabled.textContent = asset.enabled ? 'да' : 'нет';
}

function renderTable() {
  const tbody = $('#assets');
  const alive = new Set();
  for (const asset of state.assets) {
    alive.add(asset.id);
    let tr = state.rows.get(asset.id);
    if (!tr) {
      tr = createRow(asset.id);
      state.rows.set(asset.id, tr);
      tbody.appendChild(tr);
    }
    updateRow(tr, asset);
  }
  for (const [id, tr] of state.rows) {
    if (alive.has(id)) continue;
    tr.remove();
    state.rows.delete(id);
    if (state.selectedId === id) state.selectedId = null;
    if (state.editingId === id) state.editingId = null;
  }
  $('#empty').hidden = state.assets.length > 0;
}

// Редактирование цели прямо в ячейке: Enter — сохранить, Esc — отменить.
function startEdit(id) {
  const asset = state.assets.find((a) => a.id === id);
  const tr = state.rows.get(id);
  if (!asset || !tr || state.editingId !== null) return;

  const cell = tr.children[1];
  const input = document.createElement('input');
  input.className = 'cell-edit';
  input.inputMode = 'decimal';
  input.value = String(asset.target);
  state.editingId = id;
  cell.replaceChildren(input);
  input.focus();
  input.select();

  let finished = false;
  const finish = async (save) => {
    if (finished) return;
    finished = true;
    state.editingId = null;
    const raw = input.value.trim().replace(',', '.');
    const value = Number(raw);
    const valid = raw !== '' && Number.isFinite(value) && value > 0;
    cell.textContent = formatPrice(save && valid ? value : asset.target);

    if (!save || value === asset.target) return;
    if (!valid) return setMessage(addMessage, 'Цель должна быть числом больше нуля');
    const response = await request('POST', '/api/assets/threshold', { id, target: raw });
    if (!response.ok) setMessage(addMessage, response.status ? 'Не удалось изменить цель' : OFFLINE);
    else setMessage(addMessage, `Цель ${asset.symbol}: ${formatPrice(value)}`, 'info');
    poll();
  };
  input.addEventListener('keydown', (event) => {
    if (event.key === 'Enter') finish(true);
    if (event.key === 'Escape') finish(false);
  });
  input.addEventListener('blur', () => finish(true));
  input.addEventListener('click', (event) => event.stopPropagation());
}

// ---------- Алерты ----------

function onAlert(alert) {
  const item = document.createElement('li');
  for (const [cls, text] of [
    ['t', formatTime(alert.ts)],
    ['s', alert.symbol],
    ['v', `${formatPrice(alert.price)} ≤ ${formatPrice(alert.threshold)}`],
  ]) {
    const span = document.createElement('span');
    span.className = cls;
    span.textContent = text;
    item.appendChild(span);
  }
  const log = $('#log');
  log.prepend(item);
  while (log.children.length > MAX_LOG) log.lastElementChild.remove();

  const title = `${alert.symbol}: цена достигла цели`;
  const body = `Цена ${formatPrice(alert.price)} ≤ цель ${formatPrice(alert.threshold)}`;
  showToast(title, body);
  if ('Notification' in window && Notification.permission === 'granted') {
    try { new Notification(title, { body, tag: `alert-${alert.seq}` }); } catch { /* браузер запретил */ }
  }
}

function showToast(title, body) {
  const toast = document.createElement('div');
  toast.className = 'toast';
  const strong = document.createElement('b');
  strong.textContent = title;
  const text = document.createElement('span');
  text.textContent = body;
  toast.append(strong, text);
  const container = $('#toasts');
  container.appendChild(toast);
  while (container.children.length > 4) container.firstElementChild.remove();
  setTimeout(() => toast.remove(), 6000);
}

// ---------- График ----------

function appendHistory(assets) {
  const alive = new Set();
  for (const asset of assets) {
    alive.add(asset.id);
    if (asset.price === null) continue;
    let points = state.history.get(asset.id);
    if (!points) state.history.set(asset.id, (points = []));
    if (!points.length || points[points.length - 1].t < asset.ts) {
      points.push({ t: asset.ts, p: asset.price });
      if (points.length > MAX_POINTS) points.shift();
    }
  }
  for (const id of state.history.keys()) if (!alive.has(id)) state.history.delete(id);
}

const css = (name) => getComputedStyle(document.documentElement).getPropertyValue(name).trim();

function renderChart() {
  const canvas = $('#chart');
  const box = canvas.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  const w = Math.max(1, Math.round(box.width));
  const h = Math.max(1, Math.round(box.height));
  if (canvas.width !== w * dpr || canvas.height !== h * dpr) {
    canvas.width = w * dpr;
    canvas.height = h * dpr;
  }
  const ctx = canvas.getContext('2d');
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, w, h);

  const title = $('#chart-title');
  const asset = state.assets.find((a) => a.id === state.selectedId);
  if (!asset) {
    title.textContent = 'Выберите актив в таблице';
    return;
  }
  const points = state.history.get(asset.id) || [];
  if (!points.length) {
    title.textContent = `${asset.symbol} — ждём первые котировки…`;
    return;
  }
  title.textContent = asset.symbol;

  const colors = { muted: css('--muted'), grid: css('--border'), accent: css('--accent'), down: css('--down') };
  const pad = { l: 8, r: 92, t: 10, b: 26 };
  const plotW = w - pad.l - pad.r;
  const plotH = h - pad.t - pad.b;

  const minT = points[0].t;
  let maxT = points[points.length - 1].t;
  if (maxT - minT < 1000) maxT = minT + 1000;
  let lo = Math.min(asset.target, ...points.map((p) => p.p));
  let hi = Math.max(asset.target, ...points.map((p) => p.p));
  const spread = hi > lo ? (hi - lo) * 0.1 : Math.abs(hi) * 0.001 || 1;
  lo -= spread;
  hi += spread;

  const x = (t) => pad.l + (t - minT) / (maxT - minT) * plotW;
  const y = (v) => pad.t + (hi - v) / (hi - lo) * plotH;

  // Сетка и подписи осей.
  ctx.font = '11px Consolas, "Cascadia Mono", monospace';
  ctx.lineWidth = 1;
  ctx.strokeStyle = colors.grid;
  ctx.fillStyle = colors.muted;
  ctx.textAlign = 'left';
  ctx.textBaseline = 'middle';
  for (let i = 0; i <= 4; i++) {
    const v = lo + (hi - lo) * i / 4;
    const yy = Math.round(y(v)) + 0.5;
    ctx.beginPath();
    ctx.moveTo(pad.l, yy);
    ctx.lineTo(w - pad.r, yy);
    ctx.stroke();
    ctx.fillText(formatPrice(v), w - pad.r + 8, yy);
  }
  ctx.textBaseline = 'top';
  for (let i = 0; i <= 4; i++) {
    const t = minT + (maxT - minT) * i / 4;
    ctx.textAlign = i === 0 ? 'left' : i === 4 ? 'right' : 'center';
    ctx.fillText(formatTime(t), x(t), h - pad.b + 8);
  }

  // Линия цены с заливкой.
  ctx.beginPath();
  points.forEach((p, i) => (i ? ctx.lineTo(x(p.t), y(p.p)) : ctx.moveTo(x(p.t), y(p.p))));
  ctx.strokeStyle = colors.accent;
  ctx.lineWidth = 2;
  ctx.lineJoin = 'round';
  ctx.stroke();
  ctx.lineTo(x(points[points.length - 1].t), h - pad.b);
  ctx.lineTo(x(points[0].t), h - pad.b);
  ctx.closePath();
  const gradient = ctx.createLinearGradient(0, pad.t, 0, h - pad.b);
  gradient.addColorStop(0, 'rgba(59, 130, 246, 0.22)');
  gradient.addColorStop(1, 'rgba(59, 130, 246, 0)');
  ctx.fillStyle = gradient;
  ctx.fill();

  // Линия цели.
  const ty = Math.round(y(asset.target)) + 0.5;
  ctx.setLineDash([6, 4]);
  ctx.strokeStyle = colors.down;
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  ctx.moveTo(pad.l, ty);
  ctx.lineTo(w - pad.r, ty);
  ctx.stroke();
  ctx.setLineDash([]);

  // Метка последней цены справа.
  const last = points[points.length - 1];
  const ly = y(last.p);
  ctx.fillStyle = colors.accent;
  ctx.fillRect(w - pad.r + 2, ly - 9, pad.r - 4, 18);
  ctx.fillStyle = '#fff';
  ctx.textAlign = 'left';
  ctx.textBaseline = 'middle';
  ctx.fillText(formatPrice(last.p), w - pad.r + 8, ly);
  ctx.beginPath();
  ctx.arc(x(last.t), ly, 3.5, 0, Math.PI * 2);
  ctx.fill();
}

window.addEventListener('resize', renderChart);

// ---------- Старт ----------

(async function init() {
  if (location.protocol === 'file:') {
    // Страницу открыли двойным щелчком по index.html: без сервера вход не работает.
    $('#auth').hidden = false;
    resetAuth();
    for (const form of document.querySelectorAll('#auth form')) {
      setMessage($('.msg', form), 'Страница открыта как файл. Запустите run.bat — он откроет http://localhost:8080');
    }
    return;
  }
  const response = await request('GET', '/api/me');
  if (response.ok && response.data.user) return enterDashboard(response.data.user);
  $('#auth').hidden = false;
  resetAuth();
})();
