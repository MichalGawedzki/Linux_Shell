all: compile
compile:
	gcc shell.c -o shell -lreadline
run:
	gcc shell.c -o shell -lreadline && ./shell
test:
	ls -l
	ps -a | sort
    
