1 #alloc inode (directory)
1
6 #write inode
1 0 777
5 #read inode
1 0
1 #alloc inode (regular file)
2
6 #write inode
2 0 777
5 #read inode
2 0
1 #alloc inode (directory)
1
6 #write inode
3 0 777
5 #read inode
3 0
1 #alloc inode (directory)
1
6 #write inode
4 0 777
5 #read inode
4 0
1 #alloc inode (regular file)
2
6 #write inode
5 0 777
5 #read inode
5 0
1 #alloc inode (directory)
1
6 #write inode
6 0 777
5 #read inode
6 0
1 #alloc inode (directory)
1
6 #write inode
7 0 777
5 #read inode
7 0
15 # add dir entry
0 1 da1
15 # add dir entry
0 2 fa2
15 # add dir entry
0 3 da3
15 # add dir entry
1 4 db1
15 # add dir entry
1 5 fb1
15 # add dir entry
3 6 db3
15 # add dir entry
3 5 fb3
15 # add dir entry
4 7 db3
5 #read inode
0 0
5 #read inode
1 0
5 #read inode
2 0
5 #read inode
3 0
5 #read inode
4 0
5 #read inode
5 0
5 #read inode
6 0
5 #read inode
7 0
17 # rename dir entry
0 fa2
faa2
17 # rename dir entry
4 db3
dbb3
17 # rename dir entry (it is going to fail - there is no entry with that name in the directory)
4 db3
dbb3
6 #write inode
4 0 444
5 #read inode
4 0
17 # rename dir entry (it is going to fail - no executition permission in the directory)
4 db3
dbb3
6 #write inode
4 0 555
5 #read inode
4 0
17 # rename dir entry (it is going to fail - no write permission in the directory)
4 db3
dbb3
16 # remove dir entry (it is going to fail - no write permission in the directory)
4 dbb3
6 #write inode
4 0 777
5 #read inode
4 0
16 # remove dir entry
4 dbb3
5 #read inode
4 0
16 # remove dir entry (it is going to fail - it has been already removed)
4 dbb3
5 #read inode
4 0
16 # remove dir entry (it is going to fail - directory is not empty)
0 da1
16 # remove dir entry
1 db1
16 # remove dir entry
1 fb1
16 # remove dir entry
0 da1
16 # remove dir entry (it is going to fail - it is not a directory)
2 da1
16 # remove dir entry
0 faa2
16 # remove dir entry
3 db3
16 # remove dir entry
3 fb3
16 # remove dir entry
0 da3
0
