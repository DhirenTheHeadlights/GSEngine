// gse-mcp: the editor's select API for agent chats, served over MCP on stdio.
//
// Claude Code starts this as a child of the chat's `claude` process (the editor passes
// --mcp-config pointing at mcp.json next to this file). Each tool returns a bounded, shaped
// payload so a chat never has to dump a log or poll a file to learn something the editor
// already knows. Build and hibernate ride the existing file inbox in
// %LOCALAPPDATA%\GSE\cache\agent-build, exactly as Tools/gse-build and Tools/gse-hibernate
// do, so the editor needs no new endpoint for them.
//
// No dependencies beyond node. Protocol: JSON-RPC 2.0, one message per line, MCP 2025-06-18.

import { existsSync, mkdirSync, readdirSync, readFileSync, renameSync, rmSync, statSync, writeFileSync } from 'node:fs';
import { homedir } from 'node:os';
import { basename, dirname, join, resolve } from 'node:path';
import { createInterface } from 'node:readline';

const state_root = process.env.GSE_STATE_DIR
	?? join(process.env.LOCALAPPDATA ?? join(homedir(), 'AppData', 'Local'), 'GSE');
const inbox = join(state_root, 'cache', 'agent-build');
const logs_dir = join(state_root, 'logs');

// The chat's session id is how the editor attributes requests and wakes the right chat.
// Claude Code exports it to every child; GSE_AGENT_ID is the editor's override.
const agent = process.env.GSE_AGENT_ID ?? process.env.CLAUDE_CODE_SESSION_ID ?? '';

const windows_path = (path) => resolve(path).replaceAll('\\', '/');

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

const build = async (args) => {
	const project = args.project ? windows_path(args.project) : project_of(process.cwd());
	if (!agent) {
		return text_result(header(project, {
			error: 'no agent id: CLAUDE_CODE_SESSION_ID is not set in this server\'s environment, so the editor could not attribute the build.',
		}), true);
	}
	const id = new_id();
	const target = args.target === 'editor' ? 'editor' : 'game';
	const timeout = Math.max(1, Number(args.timeout ?? 1800));
	const wait = Math.max(1, Number(args.wait ?? 30));

	rmSync(join(inbox, 'results', `${id}.txt`), { force: true });
	write_atomically(join(inbox, 'requests'), id, [
		`id ${id}`,
		`agent ${agent}`,
		`target ${target}`,
		`run ${args.run ? 1 : 0}`,
		`tree ${args.tree ?? ''}`,
		`profile ${args.profile ?? ''}`,
		`cwd ${windows_path(process.cwd())}`,
		`project ${project}`,
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
						next: 'Your request keeps its place in the queue. Do not poll. Call gse_hibernate with what to do next and end your turn; the editor wakes you with the result.',
					}));
				}
			}
			else {
				result.remove();
				const outcome = result.status === 'ok' ? 'succeeded'
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
						: undefined,
				}), outcome !== 'succeeded');
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
	const hibernating = list(join(inbox, 'hibernate')).map((r) => ({ id: r.id, yours: r.agent === agent }));
	return text_result(header(project, {
		queued_requests: queued,
		hibernating_chats: hibernating.length,
		you_are_hibernating: hibernating.some((h) => h.yours),
		note: 'Requests disappear from the queue when the editor picks them up. A build in progress is visible in the editor; its result comes back to the requester, or wakes hibernating chats whose edited files it covered.',
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
	return text_result(header(project, { outcome: 'no_editor', error: 'no editor answered within 15s - it is not running, so nothing would wake you.' }), true);
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
	// Logs are one file per process, <exe>.<pid>.log, kept for the last few runs. The
	// newest for an executable is the run the chat almost always means.
	const exe = String(args.exe ?? 'Editor');
	const runs = names
		.filter((name) => name.toLowerCase().startsWith(`${exe.toLowerCase()}.`))
		.map((name) => ({ name, pid: name.split('.')[1], modified: statSync(join(logs_dir, name)).mtimeMs }))
		.sort((a, b) => b.modified - a.modified);
	if (runs.length === 0) {
		const exes = [...new Set(names.map((name) => name.split('.')[0]))].sort();
		return text_result(header(project, { error: `no log for '${exe}'`, available_exes: exes }), true);
	}
	const run = args.pid ? runs.find((r) => r.pid === String(args.pid)) : runs[0];
	if (!run) {
		return text_result(header(project, { error: `no ${exe} log for pid ${args.pid}`, runs: runs.map((r) => ({ pid: r.pid, modified: new Date(r.modified).toISOString() })) }), true);
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
		pid: run.pid,
		other_runs: runs.filter((r) => r !== run).slice(0, 4).map((r) => ({ pid: r.pid, modified: new Date(r.modified).toISOString() })),
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

const tools = {
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
				run: { type: 'boolean', description: 'Launch a play session after a successful game build.' },
				profile: { type: 'string', description: 'Named build profile from the editor build row. Omit to build the editor\'s own configuration.' },
				tree: { type: 'string', description: 'Build in a named worktree instead of the one you are in.' },
				project: { type: 'string', description: 'Directory holding the .gseproj to address. Defaults to the nearest one above the current directory.' },
				timeout: { type: 'number', description: 'Seconds before giving up (default 1800).' },
				wait: { type: 'number', description: 'Seconds to hold the line while another chat\'s build blocks yours before returning "deferred" (default 30).' },
			},
		},
		run: build,
	},
	gse_build_status: {
		description: 'What is queued in the editor build inbox and whether you are hibernating. Use instead of polling file timestamps.',
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
	gse_log_query: {
		description:
			'Query a GSE log (Editor.log, or a game exe log) with filters, returning at most `tail` matching lines. Use instead of reading the log file: logs are cleared each run and can be large.',
		schema: {
			type: 'object',
			properties: {
				exe: { type: 'string', description: 'Executable stem, e.g. "Editor" (default) or "HumanoidLocomotion". The newest run is used unless pid is given.' },
				pid: { type: 'string', description: 'Pick a specific run by process id; other_runs in a response lists the alternatives.' },
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
				'GSE editor tools. Build with gse_build (never cmake/ninja/compilers). If it returns "deferred", call gse_hibernate and end your turn. Read logs with gse_log_query, not by opening the file. Look code up with gse_symbol_query before reading a source file to find a symbol.',
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
