{
	A[++I]=$1;
	print "const char CODE_"I"[] PROGMEM = \""$2"\";"
}
END {
	print "static const struct { const char c; PGM_P seq; } codes[] = {";
	for (I in A) {
		print "{ '" A[I] "', CODE_"I" },";
	};
	print "};";
}
