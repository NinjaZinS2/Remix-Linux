#pragma once
// Discord: o processo do bot (Node.js 22.12+ com discord.js e @discordjs/voice). O Remix grava este arquivo
// na pasta do bot a cada inicio e roda "node bot.mjs". Ele e so uma ponte: quem decide fila, votacao,
// permissoes, busca e audio e o Remix (discord_host.h). Regras:
//  - o token chega so pelo stdin e nunca e gravado, mostrado ou mandado ao Remix de volta;
//  - so toca audio de http://127.0.0.1 (o Remix entrega Ogg/Opus pronto: nada de ffmpeg/opus no Node);
//  - mensagens nunca mencionam ninguem (allowedMentions vazio): titulo de musica com @everyone nao pinga.
#include <string>

namespace dcbot {

static const char* PACKAGE_JSON = R"~~~({
  "name": "remix-discord-bot",
  "private": true,
  "type": "module",
  "description": "Ponte do bot do Discord do Remix (instalada pelo app)",
  "dependencies": { "discord.js": "14.27.0", "@discordjs/voice": "0.19.2" }
}
)~~~";

static const char* BOT_JS = R"~~~(// Remix: ponte do bot do Discord (gerado pelo Remix a cada inicio; pode apagar).
// Protocolo: uma linha JSON por mensagem. stdin: Remix -> bot. stdout: bot -> Remix.
import readline from 'node:readline';
import http from 'node:http';
import {
  Client, GatewayIntentBits, Events, PermissionFlagsBits, MessageFlags, ActivityType,
  ApplicationCommandOptionType as T, InteractionContextType, ApplicationIntegrationType,
} from 'discord.js';
import {
  joinVoiceChannel, createAudioPlayer, createAudioResource, AudioPlayerStatus,
  VoiceConnectionStatus, entersState, StreamType, NoSubscriberBehavior,
} from '@discordjs/voice';

const send = (o) => { try { process.stdout.write(JSON.stringify(o) + '\n'); } catch { /* Remix fechou */ } };
const log = (m) => send({ t: 'log', msg: String(m).slice(0, 600) });
process.on('uncaughtException', (e) => log('erro: ' + (e && e.stack || e)));
process.on('unhandledRejection', (e) => log('erro: ' + (e && e.stack || e)));

// ---- comandos (nome em ingles + traducao pt-BR: cada pessoa ve no idioma do Discord dela) ----
const N = (en, pt) => ({ name: en, name_localizations: { 'pt-BR': pt } });
const D = (en, pt) => ({ description: en, description_localizations: { 'pt-BR': pt } });
const C = (en, pt, value) => ({ name: en, name_localizations: { 'pt-BR': pt }, value });
const EFFECTS = [C('slowed', 'slow', 'slow'), C('sped up', 'speed', 'speed'), C('reverb', 'reverb', 'reverb'),
  C('bass boost', 'grave', 'bass'), C('8D', '8D', '8d'), C('all off', 'desligar todos', 'off')];
const COMMANDS = [
  { ...N('play', 'tocar'), ...D('Play a song or link (or add it to the queue)', 'Toca uma música ou link (ou põe na fila)'), options: [
    { type: T.String, ...N('query', 'musica'), ...D('Name or link: YouTube, YouTube Music, SoundCloud, Spotify, Deezer, Apple Music', 'Nome ou link: YouTube, YouTube Music, SoundCloud, Spotify, Deezer, Apple Music'), required: true, max_length: 300 },
    { type: T.Boolean, ...N('next', 'proxima'), ...D('Play right after the current song (DJ)', 'Tocar logo depois da atual (DJ)') } ] },
  { ...N('search', 'buscar'), ...D('Search and pick what to play', 'Busca e escolhe o que tocar'), options: [
    { type: T.String, ...N('query', 'musica'), ...D('What to search for', 'O que buscar'), required: true, max_length: 200 },
    { type: T.String, ...N('source', 'fonte'), ...D('Where to search', 'Onde buscar'), choices: [
      { name: 'YouTube Music', value: 'ytm' }, { name: 'YouTube', value: 'yt' }, { name: 'SoundCloud', value: 'sc' } ] } ] },
  { ...N('playlist', 'playlist'), ...D("Play one of the bot owner's playlists", 'Toca uma playlist do dono do bot'), options: [
    { type: T.String, ...N('name', 'nome'), ...D('Playlist', 'Playlist'), required: true, autocomplete: true, max_length: 100 },
    { type: T.Boolean, ...N('shuffle', 'aleatorio'), ...D('Shuffle it', 'Embaralhar') } ] },
  { ...N('playlists', 'playlists'), ...D('List the playlists you can play', 'Lista as playlists liberadas') },
  { ...N('queue', 'fila'), ...D('Show the queue', 'Mostra a fila'), options: [
    { type: T.Integer, ...N('page', 'pagina'), ...D('Page', 'Página'), min_value: 1 } ] },
  { ...N('nowplaying', 'agora'), ...D('What is playing now', 'O que está tocando agora') },
  { ...N('skip', 'pular'), ...D('Skip the song (asks for a vote when needed)', 'Pula a música (com votação quando precisa)') },
  { ...N('pause', 'pausar'), ...D('Pause', 'Pausa') },
  { ...N('resume', 'continuar'), ...D('Resume', 'Continua') },
  { ...N('stop', 'parar'), ...D('Stop, clear the queue and leave', 'Para, limpa a fila e sai da chamada') },
  { ...N('remove', 'remover'), ...D('Remove a song from the queue', 'Tira uma música da fila'), options: [
    { type: T.Integer, ...N('position', 'posicao'), ...D('Position shown in /queue', 'Posição mostrada em /fila'), required: true, min_value: 1 } ] },
  { ...N('move', 'mover'), ...D('Move a song in the queue', 'Muda uma música de lugar na fila'), options: [
    { type: T.Integer, ...N('from', 'de'), ...D('Current position', 'Posição atual'), required: true, min_value: 1 },
    { type: T.Integer, ...N('to', 'para'), ...D('New position', 'Nova posição'), required: true, min_value: 1 } ] },
  { ...N('shuffle', 'embaralhar'), ...D('Shuffle the queue', 'Embaralha a fila') },
  { ...N('clear', 'limpar'), ...D('Clear the queue', 'Limpa a fila') },
  { ...N('loop', 'repetir'), ...D('Repeat mode', 'Modo de repetição'), options: [
    { type: T.String, ...N('mode', 'modo'), ...D('Mode', 'Modo'), required: true, choices: [
      C('off', 'desligado', 'off'), C('song', 'música', 'track'), C('queue', 'fila', 'queue') ] } ] },
  { ...N('seek', 'avancar'), ...D('Jump to a time (e.g. 1:30)', 'Pula para um tempo (ex.: 1:30)'), options: [
    { type: T.String, ...N('time', 'tempo'), ...D('mm:ss or seconds', 'mm:ss ou segundos'), required: true, max_length: 10 } ] },
  { ...N('effect', 'efeito'), ...D('Slowed, sped up, reverb, bass boost or 8D', 'Slow, speed, reverb, grave ou 8D'), options: [
    { type: T.String, ...N('type', 'tipo'), ...D('Effect', 'Efeito'), required: true, choices: EFFECTS },
    { type: T.Integer, ...N('level', 'nivel'), ...D('0 = off, 1 to 3', '0 = desligado, 1 a 3'), min_value: 0, max_value: 3 } ] },
  { ...N('volume', 'volume'), ...D('Bot volume (DJ)', 'Volume do bot (DJ)'), options: [
    { type: T.Integer, ...N('percent', 'porcento'), ...D('10 to 150', '10 a 150'), required: true, min_value: 10, max_value: 150 } ] },
  { ...N('leave', 'sair'), ...D('Leave the voice channel', 'Sai da chamada') },
  { ...N('help', 'ajuda'), ...D('How to use the bot', 'Como usar o bot') },
].map((c) => ({ ...c, contexts: [InteractionContextType.Guild], integration_types: [ApplicationIntegrationType.GuildInstall] }));
const PRIVATE_CMDS = new Set(['search', 'help']);

// ---- estado ------------------------------------------------------------------------------
let client = null;
let owners = [];
let playlists = [];                       // [{id, name}] para o autocompletar
const pending = new Map();                // iid -> { i, at }  (o Remix responde depois)
const voice = new Map();                  // guildId -> { conn, player, gen, req, wired }
let seq = 0;
setInterval(() => { const now = Date.now(); for (const [k, v] of pending) if (now - v.at > 14 * 60 * 1000) pending.delete(k); }, 60 * 1000).unref();

const clean = (d) => {
  const o = {};
  if (typeof d.content === 'string') o.content = d.content.slice(0, 2000);
  if (Array.isArray(d.embeds)) o.embeds = d.embeds.slice(0, 10);
  if (Array.isArray(d.components)) o.components = d.components.slice(0, 5);
  o.allowedMentions = { parse: [] };
  return o;
};
const humansIn = (guild, channelId) => {
  if (!guild || !channelId) return 0;
  let n = 0;
  for (const s of guild.voiceStates.cache.values()) {
    if (s.channelId !== channelId || s.id === client.user.id) continue;
    const u = s.member ? s.member.user : client.users.cache.get(s.id);
    if (u && u.bot) continue;
    n++;
  }
  return n;
};
const botChannel = (guild) => (guild && guild.members.me && guild.members.me.voice ? guild.members.me.voice.channelId || '' : '');
const guildList = () => [...client.guilds.cache.values()].map((g) => ({ id: g.id, name: g.name }));

// ---- audio -------------------------------------------------------------------------------
function voiceFor(guildId) {
  let v = voice.get(guildId);
  if (v) return v;
  v = { conn: null, player: createAudioPlayer({ behaviors: { noSubscriber: NoSubscriberBehavior.Play, maxMissedFrames: 250 } }), gen: 0, req: null, wired: null };
  v.player.on('stateChange', (o, n) => {
    const og = o.resource && o.resource.metadata ? o.resource.metadata.gen : 0;
    const ng = n.resource && n.resource.metadata ? n.resource.metadata.gen : 0;
    if (o.status !== AudioPlayerStatus.Idle && n.status === AudioPlayerStatus.Idle) send({ t: 'state', guild: guildId, gen: og, s: 'idle' });
    else if (n.status === AudioPlayerStatus.Playing && (o.status === AudioPlayerStatus.Buffering || o.status === AudioPlayerStatus.Idle)) send({ t: 'state', guild: guildId, gen: ng, s: 'playing' });
  });
  v.player.on('error', (e) => send({ t: 'state', guild: guildId, gen: e.resource && e.resource.metadata ? e.resource.metadata.gen : 0, s: 'error', err: String(e.message || e).slice(0, 200) }));
  voice.set(guildId, v);
  return v;
}
async function join(guildId, channelId) {
  const guild = client.guilds.cache.get(guildId);
  if (!guild) throw new Error('servidor');
  const ch = guild.channels.cache.get(channelId);
  if (!ch || !ch.isVoiceBased()) throw new Error('canal');
  const perms = ch.permissionsFor(guild.members.me);
  if (!perms || !perms.has(PermissionFlagsBits.ViewChannel) || !perms.has(PermissionFlagsBits.Connect)) throw new Error('sem_permissao_entrar');
  if (!perms.has(PermissionFlagsBits.Speak)) throw new Error('sem_permissao_falar');
  if (ch.full && !perms.has(PermissionFlagsBits.MoveMembers)) throw new Error('canal_cheio');
  const v = voiceFor(guildId);
  const conn = joinVoiceChannel({ channelId, guildId, adapterCreator: guild.voiceAdapterCreator, selfDeaf: true, selfMute: false });
  if (v.wired !== conn) {
    v.wired = conn;
    conn.on(VoiceConnectionStatus.Disconnected, async () => {
      try {
        await Promise.race([entersState(conn, VoiceConnectionStatus.Signalling, 5000), entersState(conn, VoiceConnectionStatus.Connecting, 5000)]);
      } catch {
        try { conn.destroy(); } catch { /* ja destruida */ }
      }
    });
    conn.on(VoiceConnectionStatus.Destroyed, () => {
      const cur = voice.get(guildId);
      if (cur && cur.conn === conn) { if (cur.req) cur.req.destroy(); cur.player.stop(true); voice.delete(guildId); send({ t: 'left', guild: guildId }); }
    });
    conn.on('error', (e) => log('voz: ' + (e && e.message)));
  }
  v.conn = conn;
  conn.subscribe(v.player);
  await entersState(conn, VoiceConnectionStatus.Ready, 20000);
  return v;
}
const LOCAL = /^http:\/\/127\.0\.0\.1:\d{1,5}\/[0-9a-f]{32}\/[0-9a-f]{24}$/;
function play(guildId, url, gen) {
  const v = voice.get(guildId);
  if (!v || !v.conn) { send({ t: 'state', guild: guildId, gen, s: 'error', err: 'sem_conexao' }); return; }
  if (!LOCAL.test(url)) { send({ t: 'state', guild: guildId, gen, s: 'error', err: 'url' }); return; }
  v.gen = gen;
  if (v.req) { v.req.destroy(); v.req = null; }
  const req = http.get(url, { timeout: 60000, agent: false }, (res) => {
    if (v.gen !== gen) { res.destroy(); return; }
    if (res.statusCode !== 200) { res.resume(); send({ t: 'state', guild: guildId, gen, s: 'error', err: 'http_' + res.statusCode }); return; }
    res.socket.setTimeout(0);   // pausado a musica para de ler: nao e travamento
    res.on('error', () => {});
    const resource = createAudioResource(res, { inputType: StreamType.OggOpus, metadata: { gen } });
    v.player.play(resource);
  });
  req.on('timeout', () => req.destroy(new Error('tempo_esgotado')));
  req.on('error', (e) => { if (v.gen === gen && v.req === req) send({ t: 'state', guild: guildId, gen, s: 'error', err: String(e.message || e).slice(0, 200) }); });
  v.req = req;
}

// ---- mensagens do Remix ----------------------------------------------------------------------
async function onRemix(m) {
  switch (m.t) {
    case 'login': return login(String(m.token || ''));
    case 'lists': playlists = Array.isArray(m.playlists) ? m.playlists.slice(0, 500) : []; return;
    case 'reply': {
      const p = pending.get(m.iid); if (!p) return;
      const d = clean(m.data || {});
      if (m.mode === 'private' && p.i.deferred && !p.i.ephemeral) {   // erro de um comando publico: some da conversa e so a pessoa ve
        await p.i.deleteReply().catch(() => {});
        await p.i.followUp({ ...d, flags: MessageFlags.Ephemeral });
      } else if (m.mode === 'followup') await p.i.followUp({ ...d, flags: m.ephemeral ? MessageFlags.Ephemeral : undefined });
      else await p.i.editReply(d);
      if (m.done) pending.delete(m.iid);
      return;
    }
    case 'send': {
      const ch = await client.channels.fetch(String(m.channel)).catch(() => null);
      if (!ch || !ch.isTextBased() || !ch.send) { send({ t: 'sent', key: m.key, ok: false }); return; }
      const msg = await ch.send(clean(m.data || {})).catch((e) => { log('enviar: ' + e.message); return null; });
      send({ t: 'sent', key: m.key, ok: !!msg, channel: String(m.channel), msg: msg ? msg.id : '' });
      return;
    }
    case 'edit': {
      const ch = await client.channels.fetch(String(m.channel)).catch(() => null);
      if (ch && ch.messages) await ch.messages.edit(String(m.msg), clean(m.data || {})).catch(() => {});
      return;
    }
    case 'del': {
      const ch = await client.channels.fetch(String(m.channel)).catch(() => null);
      if (ch && ch.messages) await ch.messages.delete(String(m.msg)).catch(() => {});
      return;
    }
    case 'join':
      try { await join(String(m.guild), String(m.channel)); send({ t: 'joined', guild: m.guild, channel: m.channel, ok: true, key: m.key }); }
      catch (e) { send({ t: 'joined', guild: m.guild, channel: m.channel, ok: false, err: String(e.message || e).slice(0, 100), key: m.key }); }
      return;
    case 'play': return play(String(m.guild), String(m.url), Number(m.gen) || 0);
    case 'pause': { const v = voice.get(String(m.guild)); if (v) v.player.pause(true); return; }
    case 'resume': { const v = voice.get(String(m.guild)); if (v) v.player.unpause(); return; }
    case 'stopaudio': { const v = voice.get(String(m.guild)); if (v) { v.gen = -1; if (v.req) { v.req.destroy(); v.req = null; } v.player.stop(true); } return; }
    case 'leave': { const v = voice.get(String(m.guild)); if (v && v.conn) { try { v.conn.destroy(); } catch { /* ja saiu */ } } else send({ t: 'left', guild: m.guild }); return; }
    case 'presence':
      if (client && client.user) client.user.setPresence({ activities: m.text ? [{ name: String(m.text).slice(0, 120), type: ActivityType.Listening }] : [], status: 'online' });
      return;
    case 'quit':
      for (const v of voice.values()) { try { if (v.conn) v.conn.destroy(); } catch { /* nada */ } }
      if (client) await client.destroy().catch(() => {});
      process.exit(0);
  }
}

// ---- Discord -> Remix ----------------------------------------------------------------------
function userInfo(i) {
  const m = i.member;
  const roleIds = m && m.roles && m.roles.cache ? [...m.roles.cache.keys()] : (m && Array.isArray(m.roles) ? m.roles : []);
  const roles = roleIds.map((id) => { const r = i.guild && i.guild.roles.cache.get(id); return r ? r.name : ''; }).filter(Boolean).slice(0, 50);
  return {
    id: i.user.id,
    name: String((m && m.displayName) || i.user.globalName || i.user.username).slice(0, 64),
    admin: !!(i.memberPermissions && (i.memberPermissions.has(PermissionFlagsBits.ManageGuild) || i.memberPermissions.has(PermissionFlagsBits.Administrator))),
    owner: owners.includes(i.user.id),
    roles,
  };
}
function context(i) {
  const g = i.guild;
  const uvs = g ? g.voiceStates.cache.get(i.user.id) : null;
  const bvc = botChannel(g);
  return { guild: i.guildId, gname: g ? g.name : '', channel: i.channelId, user: userInfo(i), uvc: uvs && uvs.channelId || '', bvc, humans: humansIn(g, bvc) };
}
function optionsOf(i) {
  const o = {};
  for (const x of i.options.data) {
    if (x.options) for (const y of x.options) o[y.name] = y.value;
    else o[x.name] = x.value;
  }
  return o;
}
async function onInteraction(i) {
  if (!i.inGuild() || !i.guild) { if (i.isRepliable()) await i.reply({ content: 'Use os comandos dentro de um servidor.', flags: MessageFlags.Ephemeral }).catch(() => {}); return; }
  if (i.isAutocomplete()) {
    const q = String(i.options.getFocused() || '').toLowerCase();
    const r = playlists.filter((p) => String(p.name).toLowerCase().includes(q)).slice(0, 25).map((p) => ({ name: String(p.name).slice(0, 100), value: String(p.id).slice(0, 100) }));
    await i.respond(r).catch(() => {});
    return;
  }
  const iid = (++seq).toString(36) + Date.now().toString(36);
  if (i.isChatInputCommand()) {
    await i.deferReply(PRIVATE_CMDS.has(i.commandName) ? { flags: MessageFlags.Ephemeral } : {});
    pending.set(iid, { i, at: Date.now() });
    send({ t: 'cmd', kind: 'slash', iid, name: i.commandName, opts: optionsOf(i), ...context(i) });
  } else if (i.isButton() || i.isStringSelectMenu()) {
    const id = String(i.customId || '');
    if (id.startsWith('q:')) await i.deferUpdate(); else await i.deferReply({ flags: MessageFlags.Ephemeral });
    pending.set(iid, { i, at: Date.now() });
    send({ t: 'cmd', kind: i.isButton() ? 'button' : 'select', iid, custom: id.slice(0, 100), values: i.isStringSelectMenu() ? i.values.slice(0, 25) : [], ...context(i) });
  }
}

async function login(token) {
  if (client) return;
  client = new Client({ intents: [GatewayIntentBits.Guilds, GatewayIntentBits.GuildVoiceStates] });
  client.once(Events.ClientReady, async (c) => {
    try {
      const app = await c.application.fetch();
      owners = app.owner && app.owner.members ? [...app.owner.members.keys()] : (app.owner ? [app.owner.id] : []);
      await c.application.commands.set(COMMANDS);
    } catch (e) { log('comandos: ' + e.message); }
    send({ t: 'ready', id: c.user.id, tag: c.user.tag, name: c.user.username, appId: c.application.id, owners, guilds: guildList() });
    for (const g of c.guilds.cache.values()) for (const s of g.voiceStates.cache.values()) if (owners.includes(s.id) && s.channelId) send({ t: 'ovc', guild: g.id, user: s.id, channel: s.channelId });
  });
  client.on(Events.InteractionCreate, (i) => onInteraction(i).catch((e) => log('interacao: ' + (e && e.message))));
  client.on(Events.GuildCreate, () => send({ t: 'guilds', guilds: guildList() }));
  client.on(Events.GuildDelete, () => send({ t: 'guilds', guilds: guildList() }));
  client.on(Events.VoiceStateUpdate, (o, n) => {
    const g = n.guild || o.guild; if (!g) return;
    if (owners.includes(n.id) && o.channelId !== n.channelId) send({ t: 'ovc', guild: g.id, user: n.id, channel: n.channelId || '' });
    const bvc = botChannel(g);
    if (n.id === client.user.id || (bvc && (o.channelId === bvc || n.channelId === bvc))) send({ t: 'vc', guild: g.id, channel: bvc, humans: humansIn(g, bvc) });
  });
  client.on(Events.Error, (e) => log('discord: ' + e.message));
  try {
    await client.login(token);
  } catch (e) {
    const msg = String(e && e.message || e);
    const code = e && e.code === 'TokenInvalid' ? 'token' : (/disallowed intents/i.test(msg) ? 'intents' : 'outro');
    send({ t: 'fatal', code, msg: msg.replace(token, '***').slice(0, 300) });
    process.exit(3);
  }
}

const rl = readline.createInterface({ input: process.stdin, crlfDelay: Infinity });
rl.on('line', (line) => {
  let m; try { m = JSON.parse(line); } catch { return; }
  if (m && typeof m.t === 'string') onRemix(m).catch((e) => log('remix: ' + (e && e.message)));
});
rl.on('close', () => { try { if (client) client.destroy(); } finally { process.exit(0); } });
send({ t: 'hello', node: process.versions.node });
)~~~";

} // namespace dcbot
