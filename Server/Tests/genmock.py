"""Generate stub overrides for the pure virtuals of SDK interfaces used by the component test mocks.

Usage: python genmock.py <repo root> <output header>

For each interface X, defines `MOCK_STUBS_X`, expanding to overrides that return defaults. Methods
listed in SKIP are implemented by hand in `mocks.hpp`. Generated at build time so the mocks always
match the SDK headers.
"""

import re
import sys

TARGETS = [
	("SDK/include/entity.hpp", "IEntity"),
	("SDK/include/player.hpp", "IPlayer"),
	("SDK/include/player.hpp", "IPlayerPool"),
	("SDK/include/network.hpp", "INetwork"),
	("SDK/include/core.hpp", "ILogger"),
	("SDK/include/core.hpp", "ICore"),
]

SKIP = {
	"IEntity": {"getPosition", "setPosition"},
	"IPlayer": {"getNetworkData", "getName"},
	"IPlayerPool": {
		"entries",
		"players",
		"bots",
		"getPlayerSpawnDispatcher",
		"getPlayerConnectDispatcher",
		"getPlayerStreamDispatcher",
		"getPlayerTextDispatcher",
		"getPlayerShotDispatcher",
		"getPlayerChangeDispatcher",
		"getPlayerDamageDispatcher",
		"getPlayerClickDispatcher",
		"getPlayerCheckDispatcher",
		"getPlayerUpdateDispatcher",
		"getPoolEventDispatcher",
	},
	"INetwork": {"sendRPC", "sendPacket", "getNetworkType"},
	"ILogger": {"printLn", "vprintLn", "logLn", "vlogLn", "printLnU8", "vprintLnU8"},
	"ICore": {"getPlayers", "getEventDispatcher"},
}

DECL = re.compile(
	r"virtual\s+(?P<ret>[^;{}()]*?)\b(?P<name>\w+)\s*\((?P<params>[^;{}]*)\)\s*(?P<const>const)?\s*=\s*0\s*;",
	re.S,
)


def struct_body(text, name):
	m = re.search(r"^struct %s\b[^;{]*\{" % re.escape(name), text, re.M)
	if not m:
		raise SystemExit("genmock: struct %s not found" % name)
	depth, i = 1, m.end()
	while depth:
		depth += {"{": 1, "}": -1}.get(text[i], 0)
		i += 1
	return text[m.end() : i - 1]


def strip_comments(s):
	s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
	return re.sub(r"//[^\n]*", "", s)


def strip_defaults(params):
	"""Remove default arguments, which overrides don't need."""
	out, depth, skipping = [], 0, False
	for ch in params:
		if ch in "<([{":
			depth += 1
		elif ch in ">)]}":
			depth -= 1
		if depth == 0 and ch == "=":
			skipping = True
			continue
		if depth == 0 and ch == ",":
			skipping = False
		if not skipping:
			out.append(ch)
	return " ".join("".join(out).split())


def main():
	repo, output = sys.argv[1], sys.argv[2]
	out = ["// Generated from the SDK headers by genmock.py; do not edit.", "#pragma once", ""]
	for header, name in TARGETS:
		text = strip_comments(open("%s/%s" % (repo, header), encoding="utf-8").read())
		stubs = []
		for m in DECL.finditer(struct_body(text, name)):
			if m.group("name") in SKIP.get(name, ()):
				continue
			ret = " ".join(m.group("ret").split())
			params = strip_defaults(" ".join(m.group("params").split()))
			const = " const" if m.group("const") else ""
			stubs.append("\t%s %s(%s)%s override { return mockReturn<%s>(); }" % (ret, m.group("name"), params, const, ret))
		out.append("#define MOCK_STUBS_%s \\" % name)
		out.append(" \\\n".join(stubs) if stubs else "\t/* none */")
		out.append("")
	with open(output, "w", encoding="utf-8", newline="\n") as f:
		f.write("\n".join(out))


main()
