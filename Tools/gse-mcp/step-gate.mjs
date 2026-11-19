import { readFileSync } from 'node:fs';

let refusal = '';
try {
	refusal = readFileSync(process.argv[2], 'utf8').trim();
}
catch {
	process.exit(0);
}

if (refusal) {
	process.stderr.write(refusal);
	process.exit(2);
}
