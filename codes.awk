BEGIN {
	print "static struct sequence codes[] PROGMEM = {";
}
{
	gsub("'", "\\'", $1);
	bitmask = $2;
	gsub("-", "0", bitmask);
	gsub("[.]", "1", bitmask);
	print "['"$1"'-'!'] = { "length($2)", 0b"bitmask"},";
}
END {
	print "};";
}
