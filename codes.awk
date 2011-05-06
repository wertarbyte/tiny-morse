{
	A[++I]=$1;
	print "const char CODE_"I"[] PROGMEM = \""$1$2"\";"
}
END {
	print "static PGM_P codes[] PROGMEM = {";
	for (I in A) {
		print "CODE_"I",";
	};
	print "};";
}
