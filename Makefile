main: main.cc
	g++ -O2 -Wall -Wextra -Ideps/include -Ldeps/lib $< -o $@ -lbitwuzla -lcadical -lm -lgmp -lbtor2parser
