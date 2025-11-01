## createtorrent

This is a resurrection of an old project, originally hosted [here](https://sourceforge.net/p/createtorrent/code/ci/master/tree/).

Currently, autotools have been modernized and cleaned up and many warnings in the actual code have been fixed.

#### Features changed
 - removed obsolete hidden `-p` and `-P` options.
 - `-a` (`--announce`) option is now mandatory.
 - specifying `-` as output file, writes the resulting torrent file to stdout.

#### How to build from source

 - This requires GNU autotools (a.k.a. automake autoconf etc), gcc, git and make
 - It also requires openssl (dev files) installed.

```
git clone git@github.com:felfert/createtorrent.git
cd createtorrent
make -f Makefile.am
./configure
make
```

