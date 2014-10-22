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
1 #alloc inode (symbolic link)
3
6 #write inode
8 0 777
15 # add dir entry
7 8 sl1
21 #init symbolic link
8 ../../../da3
5 #read inode
8 0
1 #alloc inode (symbolic link)
3
6 #write inode
9 0 777
15 # add dir entry
0 9 sl2
21 #init symbolic link
9 da3/fb3
5 #read inode
9 0
1 #alloc inode (symbolic link)
3
6 #write inode
10 0 777
15 # add dir entry
3 10 sl3
21 #init symbolic link
10 /fa2
5 #read inode
10 0
13 #get dir entry by path
/sl2
13 #get dir entry by path
/da1/db1/db3/sl1
13 #get dir entry by path
/da1/db1/db3/sl1/fb3
13 #get dir entry by path
/da3/sl3
0
