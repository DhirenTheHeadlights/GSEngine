// gse-mcp: the editor's select API for agent chats, served over MCP on stdio.
//
// Claude Code starts this as a child of the chat's `claude` process (the editor passes
// --mcp-config pointing at mcp.json next to this file). Each tool returns a bounded, shaped
// payload so a chat never has to dump a log or poll a file to learn something the editor
// already knows. Build and hibernate ride the existing file inbox in
// %LOCALAPPDATA%\GSE\cache\agent-build, exactly as the retired Tools/gse-build and Tools/gse-hibernate shell tools
// do, so the editor needs no new endpoint for them.
//
// No dependencies beyond node. Protocol: JSON-RPC 2.0, one message per line, MCP 2025-06-18.

import { existsSync, lstatSync, mkdirSync, readdirSync, readFileSync, renameSync, rmSync, statSync, writeFileSync } from 'node:fs';
import { homedir } from 'node:os';
import { basename, dirname, isAbsolute, join, relative, resolve } from 'node:path';
import { execFileSync } from 'node:child_process';
import { createInterface } from 'node:readline';
import { fileURLToPath } from 'node:url';

const state_root = process.env.GSE_STATE_DIR
	?? join(process.env.LOCALAPPDATA ?? join(homedir(), 'AppData', 'Local'), 'GSE');
const inbox = join(state_root, 'cache', 'agent-build');
const logs_dir = join(state_root, 'logs');

// The chat's session id is how the editor attributes requests and wakes the right chat.
// Claude Code exports it to every child; GSE_AGENT_ID is the editor's override.
const agent = process.env.GSE_AGENT_ID ?? process.env.CLAUDE_CODE_SESSION_ID ?? '';

const windows_path = (path) => resolve(path).replaceAll('\\', '/');

const engine_root = windows_path(fileURLToPath(new URL('../..', import.meta.url)));

const sleep = (ms) => new Promise((done) => setTimeout(done, ms));

const project_of = (start) => {
	let dir = resolve(start);
	while (dir) {
		let names = [];
		try {
			names = readdirSync(dir);
		}
		catch {
			return '';
		}
		if (names.some((name) => name.endsWith('.gseproj'))) {
			return windows_path(dir);
		}
		const parent = dirname(dir);
		if (parent === dir) {
			return '';
		}
		dir = parent;
	}
	return '';
};

const write_atomically = (dir, id, body) => {
	mkdirSync(dir, { recursive: true });
	const staging = join(dir, `${id}.partial`);
	writeFileSync(staging, body);
	renameSync(staging, join(dir, `${id}.txt`));
};

const read_result = (id) => {
	const path = join(inbox, 'results', `${id}.txt`);
	let text;
	try {
		text = readFileSync(path, 'utf8');
	}
	catch {
		return undefined;
	}
	if (!text) {
		return undefined;
	}
	const lines = text.split('\n');
	return {
		status: (lines[1] ?? '').replace(/^status /, ''),
		owned: Number((lines[2] ?? '').replace(/^owned /, '')),
		body: lines.slice(3).join('\n').trimEnd(),
		remove: () => rmSync(path, { force: true }),
	};
};

const new_id = () => `${process.pid}-${Math.floor(Date.now() / 1000)}-${Math.floor(Math.random() * 1e6)}`;

const header = (project, extra = {}) => ({ agent: agent || null, project: project || null, ...extra });

const text_result = (payload, is_error = false) => ({
	content: [{ type: 'text', text: typeof payload === 'string' ? payload : JSON.stringify(payload, null, 2) }],
	isError: is_error,
});

// --- tools -----------------------------------------------------------------------------

const build = async (args, run_only = false) => {
	const project = args.project ? windows_path(args.project) : project_of(process.cwd());
	if (!agent) {
		return text_result(header(project, {
			error: 'no agent id: CLAUDE_CODE_SESSION_ID is not set in this server\'s environment, so the editor could not attribute the request.',
		}), true);
	}
	const id = new_id();
	const target = run_only ? 'game' : args.target === 'editor' ? 'editor' : 'game';
	const timeout = Math.max(1, Number(args.timeout ?? 1800));
	const wait = Math.max(1, Number(args.wait ?? (run_only ? 600 : 30)));
	const settings = (Array.isArray(args.settings) ? args.settings : [])
		.map((assignment) => String(assignment).replace(/[\r\n\t]+/g, ' ').trim())
		.filter((assignment) => assignment.includes('=') && assignment.indexOf('.') < assignment.indexOf('='));
	const scenario = String(args.scenario ?? '').replace(/[\r\n\t"]+/g, '').trim();
	const seconds = Number(args.seconds ?? 0);
	const launching = run_only || Boolean(args.run);

	if (launching && !scenario && !(seconds > 0)) {
		return text_result(header(project, {
			error: 'a run needs a bound. Pass scenario: "<name>" - a scenario drives the world through a scripted sequence and exits on its own, which is what makes a run worth watching. The catalogue is Sandbox/Sandbox/Source/Sandbox/Scenarios.cppm. If you genuinely need the plain game (a boot crash, or a bug that only shows under the owner\'s settings - a scenario reads neither ini), pass seconds: N instead.',
		}), true);
	}

	rmSync(join(inbox, 'results', `${id}.txt`), { force: true });
	write_atomically(join(inbox, 'requests'), id, [
		`id ${id}`,
		`agent ${agent}`,
		`target ${target}`,
		`run ${run_only || args.run ? 1 : 0}`,
		`run_only ${run_only ? 1 : 0}`,
		`tree ${args.tree ?? ''}`,
		`profile ${args.profile ?? ''}`,
		`cwd ${windows_path(process.cwd())}`,
		`project ${project}`,
		...(scenario ? [`scenario ${scenario}`] : []),
		...(!scenario && seconds > 0 ? [`exit_after ${seconds} s`] : []),
		...settings.map((assignment) => `setting ${assignment}`),
		'',
	].join('\n'));

	const request_path = join(inbox, 'requests', `${id}.txt`);
	let picked_up = false;
	let reason = '';
	for (let waited = 0; waited < timeout; waited += 1) {
		if (!picked_up && !existsSync(request_path)) {
			picked_up = true;
		}
		const result = read_result(id);
		if (result) {
			if (result.status === 'waiting') {
				reason = result.body;
				if (waited >= wait) {
					result.remove();
					return text_result(header(project, {
						id,
						target,
						outcome: 'deferred',
						reason,
						next: run_only
							? 'A build is in flight, so your run keeps its place in the queue and will start the new executable once that build lands. Do not poll. Call gse_hibernate with what to do next and end your turn.'
							: 'Your request keeps its place in the queue. Do not poll. Call gse_hibernate with what to do next and end your turn; the editor wakes you with the result.',
					}));
				}
			}
			else {
				result.remove();
				const ok = run_only ? 'launched' : 'succeeded';
				const outcome = result.status === 'ok' ? ok
					: result.status === 'failed' ? (result.owned > 0 ? 'failed_yours' : 'failed_not_yours')
					: `editor_refused:${result.status}`;
				return text_result(header(project, {
					id,
					target,
					outcome,
					owned_errors: Number.isFinite(result.owned) ? result.owned : null,
					report: result.body,
					next: outcome === 'failed_not_yours'
						? 'None of the errors are attributed to you. Errors owned by another chat are theirs; unattributed ones are most likely another editor instance mid-refactor. Do not start fixing them.'
						: outcome === 'launched'
						? 'The game was started. It writes its own log - read it with gse_log_query (exe is the game target, newest run) rather than waiting here.'
						: undefined,
				}), outcome !== ok);
			}
		}
		if (!picked_up && waited >= 10) {
			rmSync(request_path, { force: true });
			return text_result(header(project, {
				id,
				outcome: 'no_editor',
				error: 'no editor picked up the request within 10s - it is not running or has no editor open on this project. Ask the user to start one; do not build directly.',
			}), true);
		}
		await sleep(1000);
	}
	rmSync(request_path, { force: true });
	rmSync(join(inbox, 'results', `${id}.txt`), { force: true });
	return text_result(header(project, { id, outcome: 'timeout', timeout }), true);
};

const package_sdk = async (args) => {
	const project = args.project ? windows_path(args.project) : project_of(process.cwd());
	const id = new_id();
	const timeout = Math.max(1, Number(args.timeout ?? 900));

	rmSync(join(inbox, 'results', `${id}.txt`), { force: true });
	write_atomically(join(inbox, 'package'), id, [
		`id ${id}`,
		`agent ${agent}`,
		`cwd ${windows_path(process.cwd())}`,
		`project ${project}`,
		'',
	].join('\n'));

	const request_path = join(inbox, 'package', `${id}.txt`);
	for (let waited = 0; waited < timeout; waited += 1) {
		const result = read_result(id);
		if (result) {
			result.remove();
			const image = result.status === 'ok' ? result.body.replace(/^image /, '') : undefined;
			return text_result(header(project, {
				id,
				outcome: result.status === 'ok' ? 'succeeded' : result.status,
				image,
				report: result.status === 'ok' ? undefined : result.body,
				next: result.status === 'ok'
					? 'The image is staged and verified. The editor\'s Package SDK tab holds the full transcript; gse_log_query with pattern "sdk" shows the summary lines.'
					: undefined,
			}), result.status !== 'ok');
		}
		if (waited >= 10 && existsSync(request_path)) {
			rmSync(request_path, { force: true });
			return text_result(header(project, {
				id,
				outcome: 'no_editor',
				error: 'no editor picked up the request within 10s - it is not running or has no editor open on this project.',
			}), true);
		}
		await sleep(1000);
	}
	rmSync(request_path, { force: true });
	rmSync(join(inbox, 'results', `${id}.txt`), { force: true });
	return text_result(header(project, { id, outcome: 'timeout', timeout }), true);
};

const parse_request = (path) => {
	const entry = { id: basename(path, '.txt') };
	try {
		for (const line of readFileSync(path, 'utf8').split('\n')) {
			const space = line.indexOf(' ');
			if (space > 0) {
				entry[line.slice(0, space)] = line.slice(space + 1);
			}
		}
	}
	catch {
		return undefined;
	}
	return entry;
};

const build_status = async (args) => {
	const project = args.project ? windows_path(args.project) : project_of(process.cwd());
	const list = (dir) => {
		try {
			return readdirSync(dir).filter((name) => name.endsWith('.txt')).map((name) => parse_request(join(dir, name))).filter(Boolean);
		}
		catch {
			return [];
		}
	};
	const queued = list(join(inbox, 'requests')).map((r) => ({
		id: r.id,
		target: r.target,
		project: r.project,
		yours: r.agent === agent,
		mine_or_other: r.agent === agent ? 'yours' : 'another chat',
	}));
	const hibernate_requests = list(join(inbox, 'hibernate')).map((r) => ({ id: r.id, yours: r.agent === agent }));
	return text_result(header(project, {
		queued_requests: queued,
		pending_hibernate_requests: hibernate_requests.length,
		your_hibernate_request_pending: hibernate_requests.some((h) => h.yours),
		note: 'Requests disappear from the queue when the editor picks them up. A build in progress is visible in the editor; its result comes back to the requester, or wakes hibernating chats whose edited files it covered. The hibernate counts are UNDELIVERED REQUESTS only: a hibernation the editor has already accepted lives in the editor, so 0 pending does not mean nobody is asleep - the agents panel is where that shows.',
	}));
};

const hibernate = async (args) => {
	const project = project_of(process.cwd());
	if (!agent) {
		return text_result(header(project, { error: 'no agent id in this server\'s environment; the editor cannot wake this chat.' }), true);
	}
	const then = String(args.then ?? '').trim();
	if (!then) {
		return text_result(header(project, { error: '`then` is required: what to do once the build lands.' }), true);
	}
	const id = new_id();
	rmSync(join(inbox, 'results', `${id}.txt`), { force: true });
	write_atomically(join(inbox, 'hibernate'), id, [
		`id ${id}`,
		`agent ${agent}`,
		`cwd ${windows_path(process.cwd())}`,
		'prompt',
		then,
		'',
	].join('\n'));

	for (let waited = 0; waited < 15; waited += 1) {
		const result = read_result(id);
		if (result) {
			result.remove();
			if (result.status === 'ok') {
				return text_result(header(project, {
					outcome: 'hibernating',
					instruction: 'END YOUR TURN NOW. Do not poll, do not wait. The editor prompts you again once a build covers the files you edited, and tells you whether it passed.',
					editor_said: result.body,
				}));
			}
			return text_result(header(project, { outcome: 'refused', editor_said: result.body, next: 'Not hibernating - keep working normally.' }), true);
		}
		await sleep(1000);
	}
	rmSync(join(inbox, 'hibernate', `${id}.txt`), { force: true });
	return text_result(header(project, { outcome: 'no_editor', error: 'no editor claimed this within 15s - either no editor is open on this project, or this chat was not started from the editor agent panel, so no editor owns it. Nothing would wake you; keep working normally.' }), true);
};

const phase_done = async (args) => {
	const project = project_of(process.cwd());
	if (!agent) {
		return text_result(header(project, { error: 'no agent id in this server\'s environment; the editor cannot move this chat between phases.' }), true);
	}
	const summary = String(args.summary ?? '').trim();
	if (!summary) {
		return text_result(header(project, { error: '`summary` is required: what this phase concluded.' }), true);
	}
	const findings = Math.max(0, Math.floor(Number(args.findings ?? 0)) || 0);

	const id = new_id();
	rmSync(join(inbox, 'results', `${id}.txt`), { force: true });
	write_atomically(join(inbox, 'phases'), id, [
		`id ${id}`,
		`agent ${agent}`,
		`cwd ${windows_path(process.cwd())}`,
		`findings ${findings}`,
		'summary',
		summary,
		'',
	].join('\n'));

	for (let waited = 0; waited < 15; waited += 1) {
		const result = read_result(id);
		if (result) {
			result.remove();
			if (result.status === 'ok') {
				return text_result(header(project, {
					outcome: 'reported',
					instruction: 'END YOUR TURN NOW. Do not keep working and do not call this again. The editor decides what happens next and restarts this chat in the next phase.',
					editor_said: result.body,
				}));
			}
			return text_result(header(project, { outcome: 'refused', editor_said: result.body, next: 'This chat has no phases - keep working normally.' }), true);
		}
		await sleep(1000);
	}
	rmSync(join(inbox, 'phases', `${id}.txt`), { force: true });
	return text_result(header(project, { outcome: 'no_editor', error: 'no editor claimed this within 15s - either no editor is open on this project, or this chat was not started from the editor agent panel, so it has no phases. Keep working normally.' }), true);
};

const log_query = async (args) => {
	const project = project_of(process.cwd());
	let names;
	try {
		names = readdirSync(logs_dir).filter((name) => name.endsWith('.log'));
	}
	catch {
		return text_result(header(project, { error: `no logs directory at ${windows_path(logs_dir)}` }), true);
	}
	// Logs are one file per run, <exe>.<pid>.<stamp>.log, kept for the last 40. Windows
	// reuses pids, so the stamp is the unique run key. The newest for an executable is
	// the run the chat almost always means.
	const exe = String(args.exe ?? 'Editor');
	const runs = names
		.filter((name) => name.toLowerCase().startsWith(`${exe.toLowerCase()}.`))
		.map((name) => {
			const parts = name.slice(0, -'.log'.length).split('.');
			return { name, pid: parts[1], run: parts[2], modified: statSync(join(logs_dir, name)).mtimeMs };
		})
		.sort((a, b) => b.modified - a.modified);
	if (runs.length === 0) {
		const exes = [...new Set(names.map((name) => name.split('.')[0]))].sort();
		return text_result(header(project, { error: `no log for '${exe}'`, available_exes: exes }), true);
	}
	const wanted = args.run ?? args.pid;
	const run = wanted !== undefined
		? runs.find((r) => r.run === String(wanted) || r.pid === String(wanted))
		: runs[0];
	if (!run) {
		return text_result(header(project, { error: `no ${exe} log for run '${wanted}'`, runs: runs.map((r) => ({ run: r.run, pid: r.pid, modified: new Date(r.modified).toISOString() })) }), true);
	}
	const path = join(logs_dir, run.name);
	const text = readFileSync(path, 'utf8');
	let lines = text.split(/\r?\n/);
	if (lines.at(-1) === '') {
		lines.pop();
	}
	const total = lines.length;

	// Line shape: [timestamp][level][category][thread] message
	const fields = (line) => /^\[[^\]]*\]\[([^\]]*)\]\[([^\]]*)\]/.exec(line);
	const filters = [];
	if (args.pattern) {
		let regex;
		try {
			regex = new RegExp(String(args.pattern), 'i');
		}
		catch (error) {
			return text_result(header(project, { error: `bad pattern: ${error.message}` }), true);
		}
		filters.push((line) => regex.test(line));
	}
	if (args.level) {
		const level = String(args.level).toLowerCase();
		filters.push((line) => fields(line)?.[1].toLowerCase() === level);
	}
	if (args.category) {
		const category = String(args.category).toLowerCase();
		filters.push((line) => fields(line)?.[2].toLowerCase() === category);
	}
	if (filters.length) {
		lines = lines.filter((line) => filters.every((keep) => keep(line)));
	}
	const matched = lines.length;

	const tail = Math.max(1, Math.min(2000, Number(args.tail ?? 200)));
	const max_bytes = Math.max(1000, Math.min(200_000, Number(args.max_bytes ?? 30_000)));
	let selected = lines.slice(-tail);
	let bytes = selected.reduce((sum, line) => sum + line.length + 1, 0);
	while (bytes > max_bytes && selected.length > 1) {
		bytes -= selected[0].length + 1;
		selected = selected.slice(1);
	}

	return text_result(header(project, {
		log: windows_path(path),
		run: run.run,
		pid: run.pid,
		other_runs: runs.filter((r) => r !== run).slice(0, 4).map((r) => ({ run: r.run, pid: r.pid, modified: new Date(r.modified).toISOString() })),
		modified: new Date(run.modified).toISOString(),
		total_lines: total,
		matched_lines: matched,
		returned_lines: selected.length,
		truncated: selected.length < matched,
		hint: selected.length < matched ? 'Narrow with pattern/level/category or raise tail (max 2000) rather than reading the file.' : undefined,
		lines: selected,
	}));
};

// --- trace query -----------------------------------------------------------------------
//
// Trace "dumps" under .gse/data are captured engine stdout: one log line per event, ANSI
// colour codes around every line, units embedded in values ("0.0113 m", "(1.2 N, 3.4 N,
// 5.6 N)"), and a different step key per line family. Lines come from many threads, so
// file order is not step order. Files reach 90 MB, so this streams and keeps only what
// the caller asked for.

const strip_ansi = (line) => line.replace(/\x1b\[[0-9;]*m/g, '');

const envelope = /^\[([^\]]*)\]\[([^\]]*)\]\[([^\]]*)\]\[([^\]]*)\] (.*)$/;

const unit_suffix = /(-?\d(?:\.\d+)?(?:e[-+]?\d+)?) (?:m\/s|rad\/s|N\/m|m|N)\b/g;
const number = /-?\d+(?:\.\d+)?(?:e[-+]?\d+)?/;

// Every field a message carries, as name -> number. Tuples become name.x/.y/.z. Bare
// "name number" pairs (the physics traces) and "name=number" pairs (the trainer) both
// count; "shadow step 12" and "it 3" are fields like any other.
const parse_fields = (message) => {
	const text = message.replace(unit_suffix, '$1');
	const fields = {};
	const tuple = /([A-Za-z_][A-Za-z0-9_()]*)\s*=?\s*\(\s*(-?[\d.e+-]+)\s*,\s*(-?[\d.e+-]+)\s*,\s*(-?[\d.e+-]+)\s*\)/g;
	let rest = text;
	let match;
	while ((match = tuple.exec(text)) !== null) {
		const name = match[1];
		fields[`${name}.x`] = Number(match[2]);
		fields[`${name}.y`] = Number(match[3]);
		fields[`${name}.z`] = Number(match[4]);
		rest = rest.replace(match[0], ' ');
	}
	const scalar = new RegExp(`([A-Za-z_][A-Za-z0-9_()]*)\\s*[= ]\\s*(${number.source})(?![\\w.])`, 'g');
	while ((match = scalar.exec(rest)) !== null) {
		if (!(match[1] in fields)) {
			fields[match[1]] = Number(match[2]);
		}
	}
	return fields;
};

// The first words of a message up to its first number name its family: "shadow step",
// "cpu dual it", "locomotion_train: amp total_steps", "parity_trace: step".
const family_of = (message) => {
	const head = /^([^\d=(]*?)(?=\s*[=:]?\s*-?\d|\s*\()/.exec(message);
	return (head ? head[1] : message).trim().replace(/[:=]$/, '').trim().slice(0, 48);
};

const step_keys = ['step', 'gen', 'update', 'total_steps', 'ep', 'it', 'iteration'];

const resolve_trace = (project, args) => {
	const eval_dir = join(project, '.gse', 'data', 'eval');
	if (args.file) {
		const path = resolve(project, String(args.file));
		return existsSync(path) ? [path] : [];
	}
	if (!args.run) {
		return [];
	}
	const run = String(args.run).replace(/\.txt$/, '');
	let names;
	try {
		names = readdirSync(eval_dir);
	}
	catch {
		return [];
	}
	// A long run is rotated into name.part1.txt, name.part2.txt, ... with name.txt the live
	// tail, so chronological order is the parts ascending and the base file last.
	const part_number = (name) => Number(/\.part(\d+)\.txt$/.exec(name)?.[1] ?? Number.MAX_SAFE_INTEGER);
	const parts = names
		.filter((name) => name === `${run}.txt` || /\.part\d+\.txt$/.test(name) && name.startsWith(`${run}.part`))
		.sort((a, b) => part_number(a) - part_number(b));
	return parts.map((name) => join(eval_dir, name));
};

const list_traces = (project) => {
	const eval_dir = join(project, '.gse', 'data', 'eval');
	let names;
	try {
		names = readdirSync(eval_dir).filter((name) => name.endsWith('.txt'));
	}
	catch {
		return [];
	}
	return names
		.map((name) => {
			const stat = statSync(join(eval_dir, name));
			return { run: name.replace(/\.txt$/, ''), bytes: stat.size, modified: stat.mtimeMs };
		})
		.filter((entry) => entry.bytes > 2000)
		.sort((a, b) => b.modified - a.modified)
		.map((entry) => ({ ...entry, modified: new Date(entry.modified).toISOString() }));
};

const trace_query = async (args) => {
	const project = args.project ? windows_path(args.project) : project_of(process.cwd());
	if (!project) {
		return text_result(header(project, { error: 'no .gseproj above the current directory; pass project.' }), true);
	}
	const files = resolve_trace(project, args);
	if (files.length === 0) {
		return text_result(header(project, {
			error: args.run || args.file ? `no trace named '${args.run ?? args.file}'` : 'pass run (a name under .gse/data/eval) or file',
			runs: list_traces(project).slice(0, 40),
		}), true);
	}

	let pattern;
	if (args.pattern) {
		try {
			pattern = new RegExp(String(args.pattern), 'i');
		}
		catch (error) {
			return text_result(header(project, { error: `bad pattern: ${error.message}` }), true);
		}
	}
	const family = args.family ? String(args.family).toLowerCase() : undefined;
	const category = args.category ? String(args.category).toLowerCase() : undefined;
	const step_key = args.step_key ? String(args.step_key) : undefined;
	const step_from = args.step_from === undefined ? -Infinity : Number(args.step_from);
	const step_to = args.step_to === undefined ? Infinity : Number(args.step_to);
	const fields = Array.isArray(args.fields) ? args.fields.map(String) : undefined;
	const tail = Math.max(1, Math.min(2000, Number(args.tail ?? 100)));
	const every = Math.max(1, Number(args.every ?? 1));
	const summary_only = Boolean(args.summary);
	const raw = Boolean(args.raw);

	const families = new Map();
	const rows = [];
	const stats = new Map();
	let total_lines = 0;
	let matched = 0;
	let unparsed = 0;
	let first_time;
	let last_time;

	const note_stats = (parsed) => {
		for (const [name, value] of Object.entries(parsed)) {
			if (!Number.isFinite(value)) {
				continue;
			}
			const at = step_key ? parsed[step_key] : undefined;
			let s = stats.get(name);
			if (!s) {
				s = { count: 0, min: value, max: value, sum: 0, min_at: at, max_at: at };
				stats.set(name, s);
			}
			s.count += 1;
			s.sum += value;
			if (value < s.min) {
				s.min = value;
				s.min_at = at;
			}
			if (value > s.max) {
				s.max = value;
				s.max_at = at;
			}
		}
	};

	for (const path of files) {
		const { createReadStream } = await import('node:fs');
		const lines = createInterface({ input: createReadStream(path, { encoding: 'utf8' }), crlfDelay: Infinity });
		for await (const raw_line of lines) {
			total_lines += 1;
			const line = strip_ansi(raw_line).trim();
			if (!line || line.startsWith('===')) {
				continue;
			}
			const env = envelope.exec(line);
			const message = env ? env[5] : line;
			const fam = family_of(message).toLowerCase();
			families.set(fam, (families.get(fam) ?? 0) + 1);
			if (env) {
				first_time ??= env[1];
				last_time = env[1];
			}
			if (summary_only) {
				continue;
			}
			if (category && (!env || env[3].toLowerCase() !== category)) {
				continue;
			}
			if (family && !fam.startsWith(family)) {
				continue;
			}
			if (pattern && !pattern.test(message)) {
				continue;
			}
			const parsed = parse_fields(message);
			if (Object.keys(parsed).length === 0) {
				unparsed += 1;
				if (!raw) {
					continue;
				}
			}
			if (step_key) {
				const step = parsed[step_key];
				if (step === undefined || step < step_from || step > step_to) {
					continue;
				}
			}
			matched += 1;
			if ((matched - 1) % every !== 0) {
				continue;
			}
			note_stats(parsed);
			const row = fields ? Object.fromEntries(fields.filter((name) => name in parsed).map((name) => [name, parsed[name]])) : parsed;
			if (raw) {
				row.line = message;
			}
			if (env) {
				row.thread = env[4];
			}
			rows.push(row);
			if (rows.length > tail) {
				rows.shift();
			}
		}
	}

	const family_table = [...families].sort((a, b) => b[1] - a[1]).slice(0, 40).map(([name, count]) => ({ family: name, lines: count }));
	if (summary_only) {
		return text_result(header(project, {
			files: files.map(windows_path),
			total_lines,
			first_time,
			last_time,
			families: family_table,
			next: 'Query with family (prefix of a family name), optional pattern, step_key/step_from/step_to, fields, tail, every, or aggregate.',
		}));
	}

	if (step_key && rows.length > 1) {
		rows.sort((a, b) => (a[step_key] ?? 0) - (b[step_key] ?? 0));
	}

	const aggregates = args.aggregate
		? Object.fromEntries([...stats]
			.filter(([name]) => !fields || fields.includes(name))
			.map(([name, s]) => [name, {
				count: s.count,
				min: s.min,
				max: s.max,
				mean: Number((s.sum / s.count).toPrecision(6)),
				...(step_key ? { min_at: s.min_at, max_at: s.max_at } : {}),
			}]))
		: undefined;

	return text_result(header(project, {
		files: files.map(windows_path),
		total_lines,
		matched_lines: matched,
		unparsed_skipped: unparsed,
		returned_rows: rows.length,
		truncated: rows.length < Math.ceil(matched / every),
		hint: rows.length < Math.ceil(matched / every) ? 'Narrow with family/pattern/step range, thin with every, or ask for aggregate instead of rows.' : undefined,
		families: family ? undefined : family_table.slice(0, 12),
		aggregates,
		rows: args.aggregate && !args.rows_with_aggregate ? undefined : rows,
	}));
};

// The editor answers a symbol query out of the semantic index it already keeps for go-to-
// definition, so a chat gets the definition itself instead of a guessed slice of a file.
// The answer is one directive per line: sites/site/qualified/type/body, and body is followed
// by that many source lines prefixed with '| '.
const parse_answer = (body) => {
	const matches = [];
	const at = (index) => {
		while (matches.length <= index) {
			matches.push({ source: [] });
		}
		return matches[index];
	};
	const out = {};
	let current;
	for (const line of body.split('\n')) {
		const space = line.indexOf(' ');
		const key = space > 0 ? line.slice(0, space) : line;
		const rest = space > 0 ? line.slice(space + 1) : '';
		if (key === '|') {
			current?.source.push(rest);
			continue;
		}
		const [index, ...tail] = rest.split(' ');
		switch (key) {
			case 'error':
				out.error = rest;
				break;
			case 'indexing':
				out.indexing = rest === '1';
				break;
			case 'mode':
				out.mode = rest;
				break;
			case 'sites':
				out.returned = Number(index);
				out.total = Number(tail[0]);
				break;
			case 'site':
				current = at(Number(index));
				current.kind = tail[0];
				current.role = tail[1];
				current.line = Number(tail[2]);
				current.column = Number(tail[3]);
				current.file = tail.slice(4).join(' ');
				break;
			case 'qualified':
				at(Number(index)).qualified = tail.join(' ');
				break;
			case 'type':
				at(Number(index)).type = tail.join(' ');
				break;
			case 'body':
				current = at(Number(index));
				current.first_line = Number(tail[0]);
				current.last_line = Number(tail[1]);
				current.truncated = tail[2] === 'truncated';
				break;
		}
	}
	for (const match of matches) {
		match.source = match.source.length > 0 ? match.source.join('\n') : undefined;
	}
	return matches.length > 0 ? { ...out, matches } : out;
};

const symbol_query = async (args) => {
	const project = args.project ? windows_path(args.project) : project_of(process.cwd());
	const name = String(args.name ?? '').trim();
	const file = String(args.file ?? '').trim();
	const refs = args.references === true;
	if (!name && !file) {
		return text_result(header(project, { error: 'pass `name` for a symbol, or `file` for a file outline.' }), true);
	}
	if (refs && !name) {
		return text_result(header(project, { error: 'references needs a `name`; a file outline has nothing to find uses of.' }), true);
	}
	const id = new_id();
	const timeout = Math.min(60, Math.max(1, Number(args.timeout ?? 15)));

	rmSync(join(inbox, 'results', `${id}.txt`), { force: true });
	write_atomically(join(inbox, 'queries'), id, [
		`id ${id}`,
		`agent ${agent}`,
		`name ${name}`,
		`file ${file}`,
		`cwd ${windows_path(process.cwd())}`,
		`project ${project}`,
		`sites ${Math.max(0, Math.trunc(Number(args.max_matches ?? 0)) || 0)}`,
		`lines ${Math.max(0, Math.trunc(Number(args.max_lines ?? 0)) || 0)}`,
		`body ${args.source === false ? 0 : 1}`,
		`refs ${refs ? 1 : 0}`,
		'',
	].join('\n'));

	const deadline = Date.now() + timeout * 1000;
	while (Date.now() < deadline) {
		const result = read_result(id);
		if (result) {
			result.remove();
			const answer = parse_answer(result.body);
			return text_result(header(project, { query: name || file, ...answer }), Boolean(answer.error));
		}
		await sleep(50);
	}
	rmSync(join(inbox, 'queries', `${id}.txt`), { force: true });
	return text_result(header(project, {
		query: name || file,
		outcome: 'no_editor',
		error: `no editor answered within ${timeout}s - it is not running, or none is open on this project.`,
		next: 'Fall back to Grep and Read for this lookup; source files are not restricted.',
	}), true);
};

// A gap is the select API admitting it did not cover something. It is recorded rather than
// worked around silently, so the editor can show what chats are reaching for that no tool
// serves, instead of that need disappearing into an untyped shell command.
const report_gap = async (args) => {
	const project = args.project ? windows_path(args.project) : project_of(process.cwd());
	const need = String(args.need ?? '').trim();
	if (!need) {
		return text_result(header(project, { error: 'pass `need`: what you were trying to find or do.' }), true);
	}
	const kind = args.kind === 'capability' ? 'capability' : 'lookup';
	const id = new_id();
	write_atomically(join(inbox, 'gaps'), id, [
		`id ${id}`,
		`agent ${agent}`,
		`kind ${kind}`,
		`need ${need.replace(/[\r\n\t]+/g, ' ')}`,
		`tried ${String(args.tried ?? '').trim().replace(/[\r\n\t]+/g, ' ')}`,
		`cwd ${windows_path(process.cwd())}`,
		`project ${project}`,
		`at ${new Date().toISOString()}`,
		'',
	].join('\n'));

	return text_result(header(project, {
		id,
		outcome: 'recorded',
		kind,
		need,
		next: kind === 'capability'
			? 'Recorded for the humans. This does not grant the capability and nothing will answer it - continue with what the phase does allow, and say in your phase summary what you could not determine.'
			: 'Recorded. Carry on with Grep or Read - this does not block you and needs no answer.',
	}));
};

const owned_by_tree = ['out', '.git'];

const inside = (root, path) => {
	const rel = relative(root, path);
	return rel !== '' && !rel.startsWith('..') && !isAbsolute(rel) ? rel.replaceAll('\\', '/') : undefined;
};

const delete_file = async (args) => {
	const project = args.project ? windows_path(args.project) : project_of(process.cwd());
	const requested = String(args.file_path ?? '').trim();
	if (!requested) {
		return text_result(header(project, { error: 'pass `file_path`: the file to delete.' }), true);
	}
	const path = windows_path(requested);
	const roots = [project, engine_root].filter(Boolean);
	const rel = roots.map((root) => inside(root, path)).find((found) => found !== undefined);
	if (rel === undefined) {
		return text_result(header(project, { file_path: path, error: 'outside the project and the engine tree; this tool only deletes source-tree files.', roots }), true);
	}
	if (owned_by_tree.includes(rel.split('/')[0])) {
		return text_result(header(project, { file_path: path, error: `'${rel.split('/')[0]}/' is owned by the editor or git, not by chats.` }), true);
	}

	let stat;
	try {
		stat = lstatSync(path);
	}
	catch {
		return text_result(header(project, { file_path: path, error: 'no such file.' }), true);
	}
	if (!stat.isFile()) {
		return text_result(header(project, { file_path: path, error: 'not a regular file; directories and links are not deleted by this tool.' }), true);
	}

	let git_state;
	try {
		git_state = execFileSync('git', ['status', '--porcelain', '--ignored', '--', basename(path)], { cwd: dirname(path), encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] }).trim();
	}
	catch (error) {
		return text_result(header(project, { file_path: path, error: `git could not vouch for this file, so a delete would not be recoverable: ${String(error?.stderr ?? error?.message ?? error).trim()}` }), true);
	}
	if (git_state) {
		return text_result(header(project, {
			file_path: path,
			git_status: git_state.slice(0, 2),
			error: 'only clean, committed files are deleted: this one is untracked, ignored, or has uncommitted changes, so git could not bring it back. Ask the user to delete it.',
		}), true);
	}

	const text = readFileSync(path, 'utf8');
	const declared = /^\s*(?:export\s+)?module\s+([\w.:]+)\s*;/m.exec(text)?.[1];
	rmSync(path);

	return text_result(header(project, {
		file_path: path,
		outcome: 'deleted',
		bytes: stat.size,
		lines: text.split('\n').length,
		module: declared,
		next: declared
			? `It declared module '${declared}'. Remove every import of it before building: an importer you did not edit fails as an error that is not attributed to you.`
			: undefined,
	}));
};

const tools = {
	gse_delete_file: {
		description:
			'Delete one file from the project or engine source tree. Use this instead of rm, del or Remove-Item: the deletion shows in the editor\'s agent panel with the file path, and the editor counts it as an unbuilt edit of the chats working in that tree, so gse_hibernate wakes on the build that covers it. Build errors in files that imported it are still blamed per file and will not come back as yours. Only deletes clean, committed files, so every delete is recoverable with git checkout: untracked, ignored and modified files are refused, as are directories, links, anything under out/ or .git/, and paths outside the project and engine roots. Creating a file needs no tool - use Write. If the file declared a C++ module, the response names it: remove its imports before you build.',
		schema: {
			type: 'object',
			properties: {
				file_path: { type: 'string', description: 'The file to delete, absolute or relative to your cwd.' },
				project: { type: 'string', description: 'Project directory; defaults to the nearest .gseproj above the current directory.' },
			},
			required: ['file_path'],
		},
		run: delete_file,
	},
	gse_report_gap: {
		description:
			'Record that your tools did not cover something you needed, then carry on. Two kinds. `kind: "lookup"` (the default) is when a lookup falls back to Grep, Read or a shell because no gse_* tool fits - not for a tool that exists and merely returned nothing. `kind: "capability"` is when the phase you are in withheld something the task genuinely needed: you could not run the exe to observe a failure, could not build to check a claim, could not edit to test a hypothesis. That is how the owner learns which restrictions are costing them evidence. Returns immediately and grants nothing - it is a note for the humans, not a request, so nothing waits on it and no answer comes back. Do not use it to argue for an exception or to stall: record it, continue with what the phase does allow, and say in your phase summary what you could not determine.',
		schema: {
			type: 'object',
			properties: {
				need: { type: 'string', description: 'What you were trying to find or do, in one line.' },
				tried: { type: 'string', description: 'What you fell back to, e.g. "grep -rn over Engine/Source", or "reasoned from the existing log instead of reproducing".' },
				kind: { type: 'string', description: '"lookup" (default) for a missing query tool, "capability" for something this phase denied you.' },
				project: { type: 'string', description: 'Project directory; defaults to the nearest .gseproj above the current directory.' },
			},
			required: ['need'],
		},
		run: report_gap,
	},
	gse_symbol_query: {
		description:
			'Find code by name instead of grepping and reading files. Ask for a symbol (`name`, bare or qualified: "record_usage" or "gse::ide::agent::record_usage") and the editor answers from the semantic index behind its go-to-definition: every declaration and definition site with file, line, kind and resolved type, definitions first, each with its own source text sliced to the end of the definition. Add `references: true` for every place that symbol is USED instead - resolved by the compiler, so it finds uses through aliases and skips matching text in comments, strings and unrelated same-named symbols, which is what grep cannot do. Ask for a `file` instead to get its outline - every type, function, member and alias it defines, with lines. Prefer this to Read + sed slices, to grepping for a definition, and to grepping for callers; fall back to Grep for free text or when no editor is running.',
		schema: {
			type: 'object',
			properties: {
				name: { type: 'string', description: 'Symbol to look up. A trailing qualifier narrows it: "agent::record_usage" matches only that namespace or class.' },
				file: { type: 'string', description: 'Outline this file instead of looking up a name. Relative paths resolve against your cwd, the project, then the engine root.' },
				references: { type: 'boolean', description: 'With `name`: return where the symbol is used rather than where it is defined, one source line each, sorted by file and line. total is the full count even when max_matches caps the list. The declaration sites themselves are excluded - ask without this flag for those.' },
				source: { type: 'boolean', description: 'Include the source text of each site (default true). Set false for just the locations.' },
				max_matches: { type: 'number', description: 'Sites to return (default 20, max 200). total in the response says how many matched.' },
				max_lines: { type: 'number', description: 'Total source lines across all sites (default 120, max 2000). Earlier, better-ranked sites take from it first; a site that runs out is marked truncated.' },
				project: { type: 'string', description: 'Project directory; defaults to the nearest .gseproj above the current directory.' },
				timeout: { type: 'number', description: 'Seconds to wait for the editor (default 15).' },
			},
		},
		run: symbol_query,
	},
	gse_trace_query: {
		description:
			'Query a captured run trace under .gse/data/eval (train_*, smoke_*, parity_*, play_* logs, incl. .partN continuations) without reading the file. Lines are parsed into numeric fields (tuples become name.x/.y/.z, units stripped). Call with summary=true first to see the line families in a run, then filter by family/pattern/step range, pick fields, thin with every, or request aggregate for min/max/mean per field. Replaces cd + grep + tail on these files.',
		schema: {
			type: 'object',
			properties: {
				run: { type: 'string', description: 'Trace name under .gse/data/eval without .txt, e.g. "train_ctrl_july_gpu_80m" or "smoke_shadow_trace_pass1". Spans .part1/.part2 files.' },
				file: { type: 'string', description: 'Explicit path instead of run, relative to the project or absolute.' },
				project: { type: 'string', description: 'Project directory; defaults to the nearest .gseproj above the current directory.' },
				summary: { type: 'boolean', description: 'Only list the line families and counts in the trace.' },
				family: { type: 'string', description: 'Prefix of a family name from summary, e.g. "shadow step", "cpu dual it", "gpu joint dual gen", "locomotion_train: amp", "parity_trace".' },
				pattern: { type: 'string', description: 'Case-insensitive regex the message must match.' },
				category: { type: 'string', description: 'Log category token, e.g. "physics", "general".' },
				step_key: { type: 'string', description: 'Field that orders rows and bounds the range: step, gen, it, update, total_steps, ep. Rows are sorted by it.' },
				step_from: { type: 'number' },
				step_to: { type: 'number' },
				fields: { type: 'array', items: { type: 'string' }, description: 'Only these fields in rows and aggregates, e.g. ["it","C","lambda","pen"].' },
				tail: { type: 'number', description: 'Return at most the last N matching rows (default 100, max 2000).' },
				every: { type: 'number', description: 'Keep every Nth matching row.' },
				aggregate: { type: 'boolean', description: 'Return count/min/max/mean per numeric field over all matching rows (with the step at min/max when step_key is set) instead of rows.' },
				rows_with_aggregate: { type: 'boolean', description: 'Return rows as well as aggregates.' },
				raw: { type: 'boolean', description: 'Include the message text in each row and keep lines that parse to no fields.' },
			},
		},
		run: trace_query,
	},
	gse_build: {
		description:
			'Ask the running GSE editor to build, the same way its own build buttons do. Blocks until the build finishes (or `wait` seconds if another chat is still working, then returns outcome "deferred" - call gse_hibernate and end your turn). Errors come back split by ownership: only act on the ones attributed to you. This is the only sanctioned way to build; cmake, ninja and compilers are not available to you.',
		schema: {
			type: 'object',
			properties: {
				target: { type: 'string', enum: ['game', 'editor'], description: 'game (default) builds the project; editor rebuilds the running editor, which relaunches itself.' },
				run: { type: 'boolean', description: 'Launch a play session after a successful game build. Like gse_run, it must be bounded: pass scenario, or seconds for a free run.' },
				scenario: { type: 'string', description: 'With run: the scenario to launch afterwards. See gse_run.' },
				seconds: { type: 'number', description: 'With run: bound a free run to this long. See gse_run.' },
				profile: { type: 'string', description: 'Named build profile from the editor build row. Omit to build the editor\'s own configuration.' },
				tree: { type: 'string', description: 'Build in a named worktree instead of the one you are in.' },
				project: { type: 'string', description: 'Directory holding the .gseproj to address. Defaults to the nearest one above the current directory.' },
				timeout: { type: 'number', description: 'Seconds before giving up (default 1800).' },
				wait: { type: 'number', description: 'Seconds to hold the line while another chat\'s build blocks yours before returning "deferred" (default 30).' },
			},
		},
		run: build,
	},
	gse_package_sdk: {
		description:
			'Ask the running GSE editor to stage and verify the engine SDK image from its current build tree, the same as the "Package SDK" button. Blocks until the image is ready or the verify step fails; the full transcript lands in the editor\'s Package SDK tab. Refused while a build or another packaging run is in progress.',
		schema: {
			type: 'object',
			properties: {
				project: { type: 'string', description: 'Directory holding the .gseproj to address. Defaults to the nearest one above the current directory.' },
				timeout: { type: 'number', description: 'Seconds before giving up (default 900).' },
			},
		},
		run: package_sdk,
	},
	gse_build_status: {
		description: 'What is queued in the editor build inbox, and whether a hibernate request of yours is still undelivered. Use instead of polling file timestamps. It cannot see a hibernation the editor has already accepted - that state lives in the editor.',
		schema: { type: 'object', properties: { project: { type: 'string' } } },
		run: build_status,
	},
	gse_hibernate: {
		description:
			'Sleep until your edits are in a build, then be woken with the result and the prompt you pass here. Returns immediately; END YOUR TURN after calling it. Use when a build is coming anyway or gse_build returned "deferred". Only chats started from the editor agent panel can be woken.',
		schema: {
			type: 'object',
			properties: { then: { type: 'string', description: 'What to do once the build lands.' } },
			required: ['then'],
		},
		run: hibernate,
	},
	gse_run: {
		description:
			'Launch the existing game executable through the editor, without building it. Use it to observe real behaviour - a crash, a log, a startup failure - rather than reasoning about it from source. Every run must be bounded, so pass either `scenario` or `seconds`. A scenario is the form to reach for: it is a named script that settles the world, drives it through a fixed sequence on a fixed-step clock, and exits on its own with a profile, a percentile summary and a world-state hash - a launch with nobody at the keyboard otherwise just sits in an empty scene until something kills it. Read Sandbox/Sandbox/Source/Sandbox/Scenarios.cppm for the catalogue; a scenario run is hermetic, so it reads neither ini. `seconds` is the escape hatch for the two things a scenario cannot do: a crash during boot, and a bug that only appears under the owner\'s saved settings. It does NOT build: whatever is on disk is what runs, so if you have edited since the last build, call gse_build first or you will be watching stale code. If a build is already in flight your run waits for it and then starts the new executable, so you never race a half-written binary. `settings` applies setting overrides to this run only, as command-line arguments - nothing is written to the ini and the next run is unaffected, so this is how you turn on a diagnostic like validation for one reproduction. Returns once the game has been started, not when it exits; the game writes its own log, so read it afterwards with gse_log_query rather than waiting here. Only the game can be launched - the editor is the process answering you.',
		schema: {
			type: 'object',
			properties: {
				scenario: { type: 'string', description: 'Name of a scenario to run, e.g. "physics_stress" or "solver_showcase". The catalogue is the annotated declarations in Sandbox/Sandbox/Source/Sandbox/Scenarios.cppm. The scenario owns the scene, the frame budget and the solver, and ends the run itself. A name that is not in the catalogue exits immediately and writes no log, so take it from the file rather than guessing.' },
				seconds: { type: 'number', description: 'Run the plain game this long, then shut it down cleanly. Only for what a scenario cannot reach: a boot crash, or a bug that needs the owner\'s saved settings. Ignored when scenario is given.' },
				settings: {
					type: 'array',
					items: { type: 'string' },
					description: 'Setting overrides for this run only, each "Section.key=value", e.g. "Graphics.validation_layers_enabled=true". Applied in memory; the ini is never touched. Entries not in Section.key=value form are dropped. A scenario pins its own declared settings ahead of these.',
				},
				tree: { type: 'string', description: 'Worktree name to run; defaults to the one owning your working directory.' },
				project: { type: 'string', description: 'Project directory; defaults to the nearest .gseproj above the current directory.' },
				wait: { type: 'number', description: 'Seconds to wait for an in-flight build before giving up and returning "deferred" (default 600).' },
				timeout: { type: 'number', description: 'Seconds to wait for the editor overall (default 1800).' },
			},
		},
		run: (args) => build(args, true),
	},
	gse_phase_done: {
		description:
			'Report that the current phase of your task is finished, and let the editor decide what follows. Call it as the last thing you do in a phase, then END YOUR TURN - the editor restarts this chat in the next phase, or holds for the owner\'s approval. Do not call it repeatedly within one turn, but if you are still in the same phase on a later turn you may report again with an updated summary; the owner approves through the editor, not through chat, so asking them to approve is not something you can do on their behalf. Scoping ends when the plan is agreed and nothing is left to decide; applying ends when the change is complete; reviewing ends when you have read the whole diff, and `findings` is how many real defects you found (0 settles the task, anything else sends it back to be fixed). Only chats started from the editor agent panel have phases.',
		schema: {
			type: 'object',
			properties: {
				summary: { type: 'string', description: 'What this phase concluded, for the owner and for the next phase. The plan when scoping, what changed when applying, the findings when reviewing.' },
				findings: { type: 'number', description: 'Reviewing only: how many real defects you found. Omit or 0 means the change is sound and the task is settled.' },
			},
			required: ['summary'],
		},
		run: phase_done,
	},
	gse_log_query: {
		description:
			'Query a GSE log (the editor, or a game exe) with filters, returning at most `tail` matching lines. Use instead of reading the log file: each run writes its own file and they can be large.',
		schema: {
			type: 'object',
			properties: {
				exe: { type: 'string', description: 'Executable stem, e.g. "Editor" (default) or "HumanoidLocomotion". The newest run is used unless run or pid is given.' },
				run: { type: 'string', description: 'Pick a specific run by its launch stamp, e.g. "20260916_152751"; other_runs in a response lists the alternatives. This is the unique run key — prefer it over pid.' },
				pid: { type: 'string', description: 'Pick a specific run by process id. Windows reuses pids, so this can match several runs; the newest wins. Use run to disambiguate.' },
				pattern: { type: 'string', description: 'Case-insensitive regular expression a line must match.' },
				level: { type: 'string', description: 'Level token a line must contain, e.g. "error", "warning".' },
				category: { type: 'string', description: 'Category token a line must contain, e.g. "task", "physics".' },
				tail: { type: 'number', description: 'Return the last N matching lines (default 200, max 2000).' },
				max_bytes: { type: 'number', description: 'Byte budget for returned lines (default 30000, max 200000).' },
			},
		},
		run: log_query,
	},
};

// --- JSON-RPC over stdio -----------------------------------------------------------------

const send = (message) => {
	process.stdout.write(`${JSON.stringify(message)}\n`);
};

const reply = (id, result) => send({ jsonrpc: '2.0', id, result });
const fail = (id, code, message) => send({ jsonrpc: '2.0', id, error: { code, message } });

const handle = async (message) => {
	const { id, method, params } = message;
	if (method === 'initialize') {
		reply(id, {
			protocolVersion: params?.protocolVersion ?? '2025-06-18',
			capabilities: { tools: {} },
			serverInfo: { name: 'gse', version: '0.1.0' },
			instructions:
				'GSE editor tools. Build with gse_build (never cmake/ninja/compilers). If it returns "deferred", call gse_hibernate and end your turn. Read logs with gse_log_query, not by opening the file. Look code up with gse_symbol_query before reading a source file to find a symbol. Delete files with gse_delete_file, not a shell. If your system prompt names a current phase, end that phase with gse_phase_done rather than simply stopping.',
		});
		return;
	}
	if (method === 'ping') {
		reply(id, {});
		return;
	}
	if (method === 'tools/list') {
		reply(id, {
			tools: Object.entries(tools).map(([name, tool]) => ({ name, description: tool.description, inputSchema: tool.schema })),
		});
		return;
	}
	if (method === 'tools/call') {
		const tool = tools[params?.name];
		if (!tool) {
			fail(id, -32602, `unknown tool ${params?.name}`);
			return;
		}
		try {
			reply(id, await tool.run(params.arguments ?? {}));
		}
		catch (error) {
			reply(id, text_result({ error: String(error?.message ?? error) }, true));
		}
		return;
	}
	if (id !== undefined && !String(method).startsWith('notifications/')) {
		fail(id, -32601, `method not found: ${method}`);
	}
};

// Calls run concurrently; a closed stdin means "no more requests", not "abandon the
// ones in flight", so exit only once every started call has answered.
let in_flight = 0;
let closed = false;
const maybe_exit = () => {
	if (closed && in_flight === 0) {
		process.exit(0);
	}
};

const input = createInterface({ input: process.stdin, crlfDelay: Infinity });
input.on('line', (line) => {
	if (!line.trim()) {
		return;
	}
	let message;
	try {
		message = JSON.parse(line);
	}
	catch {
		fail(null, -32700, 'parse error');
		return;
	}
	in_flight += 1;
	handle(message)
		.catch((error) => {
			if (message.id !== undefined) {
				fail(message.id, -32603, String(error?.message ?? error));
			}
		})
		.finally(() => {
			in_flight -= 1;
			maybe_exit();
		});
});
input.on('close', () => {
	closed = true;
	maybe_exit();
});
