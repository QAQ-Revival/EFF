const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const { spawnSync } = require('node:child_process');
const root = path.resolve(__dirname, '..');
const UPSTREAM = 'https://github.com/ssice-a/EFF.git';
const OUTPUT_FOLDER = 'bin/resource-directory-entry-format3';
// Where each file of the build goes in the game directory, as in the official EFF_v<version>_dll.zip.
const LAYOUT = {
  'd3dcompiler_47.dll': `${OUTPUT_FOLDER}/d3dcompiler_47.dll`,
  'vulkan-1.dll': `${OUTPUT_FOLDER}/vulkan-1.dll`,
  'plugin/eff.dll': `${OUTPUT_FOLDER}/eff.dll`,
  'plugin/package-protocol.json': `${OUTPUT_FOLDER}/package-protocol.json`,
  'plugin/LICENSE': 'LICENSE',
  'plugin/THIRD_PARTY_NOTICES': 'THIRD_PARTY_NOTICES',
  'plugin/eff.ini': `${OUTPUT_FOLDER}/eff.ini`
};

function git(args, allowed = [0]) {
  const command = spawnSync('git', ['-c', 'credential.helper=', ...args], { cwd: root, encoding: 'utf8', windowsHide: true });
  if (command.error || !allowed.includes(command.status)) throw Error('Cannot identify the EFF source: git ' + args[0]);
  return { status: command.status, stdout: command.stdout.trim() };
}
function releaseVersion() {
  const header = fs.readFileSync(path.join(root, 'src/eiem_version.h'), 'utf8');
  const part = name => new RegExp(`^#define EIEM_VERSION_${name} (\\d+)\\s*$`, 'm').exec(header)?.[1];
  const parts = ['MAJOR', 'MINOR', 'PATCH'].map(part);
  if (parts.some(value => value === undefined)) throw Error('Cannot read the EFF version');
  return parts.join('.');
}
// The last upstream commit merged here. A checkout without the upstream remote, as in CI, fetches its branch.
function upstreamBase() {
  git(['fetch', '--quiet', '--no-tags', UPSTREAM, '+main:refs/remotes/upstream/main'], [0, 1, 128]);
  const base = git(['merge-base', 'HEAD', 'refs/remotes/upstream/main'], [0, 1, 128]);
  if (base.status || !/^[a-f0-9]{40}$/.test(base.stdout)) throw Error('Cannot find the upstream EFF commit this build is based on');
  return base.stdout;
}
// QAQR numbers its build <upstream version>.<n>, n counting the non-merge commits on top of upstream,
// fixes merged from community forks included, so a build with other changes never shares a number.
function qaqrVersion() {
  const base = upstreamBase();
  const revision = Number(git(['rev-list', '--count', '--no-merges', `${base}..HEAD`]).stdout);
  if (!Number.isSafeInteger(revision) || revision > 65535) throw Error('The EFF revision does not fit the DLL version');
  return { base, revision, version: `${releaseVersion()}.${revision}` };
}
// The fixed part of the version resource, which QAQR reads to tell builds apart.
function peVersion(bytes) {
  const fixed = bytes.indexOf(Buffer.from([0xbd, 0x04, 0xef, 0xfe]));
  if (fixed < 0 || fixed + 24 > bytes.length) return null;
  const read = offset => { const high = bytes.readUInt32LE(fixed + offset), low = bytes.readUInt32LE(fixed + offset + 4); return [high >>> 16, high & 0xffff, low >>> 16, low & 0xffff].join('.'); };
  return { file: read(8), product: read(16) };
}
function build({ output = path.join(root, 'build/component'), verifyOnly = false } = {}) {
  const { base, revision, version } = qaqrVersion();
  // build.bat finds MSVC itself and links the static runtime (/MT). cmd may not search the current directory.
  const result = spawnSync(process.env.ComSpec || 'cmd.exe', ['/d', '/s', '/c', `""${path.join(root, 'build.bat')}""`], {
    cwd: root, stdio: 'inherit', windowsHide: true, windowsVerbatimArguments: true, env: { ...process.env, EFF_VERSION_REVISION: String(revision) }
  });
  if (result.error || result.status) throw result.error || Error('The EFF build failed');
  const built = peVersion(fs.readFileSync(path.join(root, LAYOUT['plugin/eff.dll'])));
  if (built?.file !== version || built.product !== version) throw Error(`eff.dll carries version ${built?.file || 'none'}, expected ${version}`);
  if (verifyOnly) return;
  if (git(['status', '--porcelain', '--ignore-submodules=all']).stdout) throw Error('Commit the reviewed EFF source before packaging');
  output = path.resolve(output);
  fs.rmSync(output, { recursive: true, force: true });
  const files = {};
  for (const [to, from] of Object.entries(LAYOUT)) {
    const target = path.join(output, to);
    fs.mkdirSync(path.dirname(target), { recursive: true });
    fs.copyFileSync(path.join(root, from), target);
    files[to] = crypto.createHash('sha256').update(fs.readFileSync(target)).digest('hex');
  }
  fs.writeFileSync(path.join(output, 'component.json'), JSON.stringify({
    source: 'https://github.com/QAQ-Revival/EFF', sourceCommit: git(['rev-parse', 'HEAD']).stdout, version,
    upstream: { repository: 'ssice-a/EFF', commit: base }, gameRuntimeValidated: false, files
  }, null, 2) + '\n');
  return output;
}
if (require.main === module) build({ verifyOnly: process.argv.includes('--verify-only') });
module.exports = { build, qaqrVersion, peVersion, LAYOUT };
