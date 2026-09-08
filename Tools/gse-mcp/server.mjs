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

const tools = {
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
				'GSE editor tools. Build with gse_build (never cmake/ninja/compilers). If it returns "deferred", call gse_hibernate and end your turn. Read logs with gse_log_query, not by opening the file.',
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
	handle(message).catch((error) => {
		if (message.id !== undefined) {
			fail(message.id, -32603, String(error?.message ?? error));
		}
	});
});
input.on('close', () => process.exit(0));
