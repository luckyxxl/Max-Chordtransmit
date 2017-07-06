inlets = 1
outlets = 8

var chords =
[
	[24, 49, 61, 73, 92, 97, 122, 146, 183],
	[18, 36, 46, 55, 65, 73, 92, 109, 130],
	[20, 41, 49, 61, 73, 82, 97, 122, 146],
	[16, 32, 41, 49, 61, 65, 82, 103, 122]
]

function list()
{
	var l = arguments

	var c = null
	for(var i=0; i<chords.length; ++i)
	{
		if(chords[i][0] == l[0])
		{
			c = chords[i]
			break
		}
	}

	if(c === null) return

	for(var i=7; i>=0; --i)
	{
		var includes = false
		for(var j=0; j<l.length; ++j)
		{
			if(l[j] == c[i+1]) { includes = true; break }
		}
		outlet(i, includes);
	}
}