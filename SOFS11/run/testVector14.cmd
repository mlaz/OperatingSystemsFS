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
13 #get dir entry by path
/da1/db1/db3
13 #get dir entry by path
/fa2
13 #get dir entry by path
/da1/db1/db3/../../../da3/fb3
13 #get dir entry by path
/da3/./.././da3/db3
13 #get dir entry by path  (it is going to fail - fa2 is not a directory)
/da3/./.././fa2/db3
6 #write inode
3 0 666
5 #read inode
3 0
13 #get dir entry by path  (it is going to fail - da3 has no execution permission)
/da3/db3
0
