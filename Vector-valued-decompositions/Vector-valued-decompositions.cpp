#include "stdafx.h"
#include <stdlib.h>
#include <windows.h>
#include "lp_lib.h"

const int V = 10; // starting order of graphs to check.
int numTournaments[] = {1,1,1,2,4,12,56,456,6880,191536,9733056};  //number of tournaments on n vertices for n=0,...,10.
const double epsilon = 0.001;  // an accuracy parameter

int graph[V+4][V+4]; // cuurent graph. Current graph has n vertices where V <= n <= V+4
int rowNum[V+4][V+4]; // labeling the rows of the linear program
int colNum[V+4][V+4][V+4]; // labeling the variables of the linear program

int graphs[400000][V+4][V+4];

FILE *lpfile, *outfile, *logfile, *datafile = NULL;

#define DIR "d:\\research\\programs\\vector-valued-decompositions\\"
#define LPPROG "d:\\projects\\legacy\\LPSolveIDE\\lp_solve"
#define DATAFILE "d:\\research\\general\\combinatorial data\\tour"
#define LOGFILE "d:\\research\\programs\\vector-valued-decompositions\\log.txt"

HINSTANCE lpsolve;
lprec *lp;
make_lp_func *_make_lp;
delete_lp_func *_delete_lp;
set_row_func *_set_row;
set_obj_fn_func *_set_obj_fn;
solve_func *_solve;
set_maxim_func *_set_maxim;
get_objective_func *_get_objective;
print_lp_func *_print_lp;
set_rh_vec_func *_set_rh_vec;
set_verbose_func *_set_verbose;
set_constr_type_func *_set_constr_type;

void copyGraphToNext(int numNext, int n)
{
	for (int i = 0; i < n; i++)
		for (int j = 0; j < n; j++)
			graphs[numNext][i][j] = graph[i][j];
}

void setRowNum(int n)
{
	int count = 1;
	for (int i = 0; i < n; i++)
		for (int j = i + 1; j < n; j++)
		{
			rowNum[i][j] = count;
			rowNum[j][i] = count++;
		}
}

void setColNum(int n)
{
	int count = 1;
	for (int i = 0; i < n; i++)
		for (int j = i + 1; j < n; j++)
			for (int k = j + 1; k < n; k++)
			{
				colNum[i][j][k] = count;
				colNum[i][k][j] = count;
				colNum[j][i][k] = count;
				colNum[j][k][i] = count;
				colNum[k][i][j] = count;
				colNum[k][j][i] = count++;
			}
}

void nextGraphFromFile()
{
	char c;
	for (int i = 0; i < V; i++)
	{
		graph[i][i] = 0;
		for (int j = i+1; j < V; j++)
		{
			c = fgetc(datafile);
			graph[i][j] = c - '0';
			graph[j][i] = 1 - graph[i][j];
		}
	}
	fgetc(datafile);
}

bool checkTransitive(int i, int j, int k)
{
	if ((graph[i][j] == graph[j][k]) && (graph[j][k] == graph[k][i]))
		return false;
	return true;
}

void writeTargetLP(int n)
{
	int i, j, k;
	static REAL row[500];

	for (i = 0; i < n - 2; i++)
		for (j = i + 1; j < n - 1; j++)
			for (k = j + 1; k < n; k++)
				if (checkTransitive(i, j, k))
					row[colNum[i][j][k]] = 1;
				else
					row[colNum[i][j][k]] = 0;
	_set_obj_fn(lp, row);
	_set_maxim(lp);
}

void writeConstraintsLP(int n)
{
	int i, j, k;
	static REAL row[500];
	static REAL column[500];

	for (i = 0; i < n - 1; i++)
		for (j = i + 1; j < n; j++)
		{
			row[0] = 1;
			for (k = 1; k < 1 + n * (n - 1)*(n - 2) / 6; k++)
				row[k] = 0;
			for (k = 0; k < n; k++)
				if ((k != i) && (k != j))
					row[colNum[i][j][k]] = 1;
			column[rowNum[i][j]] = 1;
			_set_row(lp, rowNum[i][j], row);
		}
	_set_rh_vec(lp, column);
}




void printGraph(int n)
{
	fprintf(logfile, "\n");
	for (int i = 0; i<n; i++)
	{
		for (int j = 0; j<n; j++)
			fprintf(logfile, "%3d", graph[i][j]);
		fprintf(logfile, "\n");
	}
	fprintf(logfile, "\n");
}

void copyNextToGraph(int loc, int n)
{
	for (int i = 0; i < n-1; i++)
		for (int j = 0; j < n-1; j++)
			graph[i][j] = graphs[loc][i][j];
	for (int i = 0; i < n; i++)
	{
		graph[n-1][i] = 0;
		graph[i][n-1] = 1;
	}
	graph[n - 1][n - 1] = 0;
}

bool nextEdge(int n)
{
	for (int i = 0; i < n-1; i++)
		if (graph[n-1][i] == 0)
		{
			graph[n-1][i] = 1;
			graph[i][n-1] = 0;
			return true;
		}
		else
		{
			graph[n-1][i] = 0;
			graph[i][n-1] = 1;
		}
	return false;
}


int nextLevel(int n, int start, int num, double threshold)
{
	double min = 100;
	double result;
	int numMin = 0;
	int counter = 0;
	int numNotOptimal = 0;
	int numNext = 0;

	lp = _make_lp(n *(n - 1) / 2, n*(n - 1)*(n - 2) / 6);
	_set_verbose(lp, SEVERE);
	setRowNum(n);
	setColNum(n);
	fprintf(logfile, "Checking one vertex extensions of %d instances on %d vertices.\n", num, n);
	printf("Checking one vertex extensions of %d instances on %d vertices.\n", num, n);
	while (counter < num)
	{
		copyNextToGraph(start+counter,n);
		counter++;
		if (counter % 100 == 0)
			printf("%d\n", counter);
		do 
		{
			writeTargetLP(n);
			writeConstraintsLP(n);
			_solve(lp);
			result = _get_objective(lp);
			if (result < min - epsilon)
			{
				min = result;
				numMin = 1;
				fprintf(logfile, "New optimal value obtained: %.10f.\n", min);
				printf("New optimal value obtained: %.10f.\n", min);
			}
			else if (result <= min + epsilon && result >= min - epsilon)
				numMin++;
			if (result < n*(n - 1) / 3 - epsilon)
				numNotOptimal++;
			if (result < threshold)
			{
				copyGraphToNext(start+ numNext + num,n);
				numNext++;
			}
		} while (nextEdge(n));
	}
	fprintf(logfile, "min=%f. Non optimality in %d instances. Minimality in %d instances.\n", min, numNotOptimal, numMin);
	printf("min=%f. Non optimality in %d instances. Minimality in %d instances.\n", min, numNotOptimal, numMin);
	fprintf(logfile, "%d graphs passed to next level.\n", numNext);
	printf("%d graphs passed to next level.\n", numNext);
	_delete_lp(lp);
	return numNext;
}


int main()
{
	double min=100;
	double result;
	int counter = 0;
	char dataFileName[200];
	int numNotOptimal = 0;
	int numMin = 0;
	double threshold = 21.7;//V * (V - 1) / 3 - epsilon;
	int numNext = 0;
	int further,further1,further2,further3;

	lpsolve = LoadLibrary(TEXT("d:\\Projects\\Legacy\\LPSolveIDE\\lpsolve55.dll"));
	_make_lp = (make_lp_func *)GetProcAddress(lpsolve, "make_lp");
	_delete_lp = (delete_lp_func *)GetProcAddress(lpsolve, "delete_lp");
	_set_row = (set_row_func *)GetProcAddress(lpsolve, "set_row");
	_set_obj_fn = (set_obj_fn_func *)GetProcAddress(lpsolve, "set_obj_fn");
	_solve = (solve_func *)GetProcAddress(lpsolve, "solve");
	_set_maxim = (set_maxim_func *)GetProcAddress(lpsolve, "set_maxim");
	_get_objective = (get_objective_func *)GetProcAddress(lpsolve, "get_objective");
	_print_lp = (print_lp_func *)GetProcAddress(lpsolve, "print_lp");
	_set_rh_vec = (set_rh_vec_func *)GetProcAddress(lpsolve, "set_rh_vec");
	_set_verbose = (set_verbose_func *)GetProcAddress(lpsolve, "set_verbose");
	_set_constr_type = (set_constr_type_func *)GetProcAddress(lpsolve, "set_constr_type");

	lp = _make_lp(V*(V-1)/2, V*(V-1)*(V-2)/6);
	_set_verbose(lp, SEVERE);
	setRowNum(V);
	setColNum(V);

	sprintf_s(dataFileName, "%s%d.txt", DATAFILE, V);
	fopen_s(&logfile, LOGFILE, "w");
	fprintf(logfile, "Checking %d instances on %d vertices.\n", numTournaments[V],V);
	printf("Checking %d instances on %d vertices.\n", numTournaments[V], V);
	fopen_s(&datafile, dataFileName, "r");
	while(counter < numTournaments[V])
	{
		counter++;
		if (counter % 10000 == 0)
			printf("%d\n", counter);
		nextGraphFromFile();
		writeTargetLP(V);
		writeConstraintsLP(V);
		_solve(lp);
		result = _get_objective(lp);
		if (result < min-epsilon)
		{
			min = result;
			numMin = 1;
			fprintf(logfile, "New optimal value obtained: %.10f.\n", min);
			printf("New optimal value obtained: %.10f.\n", min);
		}
		else if (result <= min+epsilon && result >= min-epsilon)
			numMin++;
		if (result < V*(V - 1) / 6 - epsilon)
			numNotOptimal++;
		if (result < 12.86)   // 12.86 is the threshold for the case V=10
		{
			copyGraphToNext(numNext,V);
			numNext++;
		}

	}
	fprintf(logfile, "min=%f. Non optimality in %d instances. Minimality in %d instances.\n", min, numNotOptimal,numMin);
	printf("min=%f. Non optimality in %d instances. Minimality in %d instances.\n", min, numNotOptimal, numMin);
	fprintf(logfile, "%d graphs passed to next level.\n", numNext);
	printf("%d graphs passed to next level.\n", numNext);
	_delete_lp(lp);
	further =  nextLevel(V+1, 0,				numNext,	15.72);
	further1 = nextLevel(V+2, numNext,			further,	18.86);
	further2 = nextLevel(V+3, numNext+further,	further1,	22.3);
	further3 = nextLevel(V+4, numNext+further+further1, further2, 26.1);
	fclose(logfile);
	FreeLibrary(lpsolve);
    return 0;
}