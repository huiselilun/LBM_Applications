/* The LBM code for a heated lid-driven cavity */
#include <iostream>
#include <omp.h>
#include <fstream>
#include <string>

#define NUM_THREADS  4

using namespace std;

const int n = 100, m = 100;
const int mstep = 20000;

double f[9][n + 1][m + 1];
double feq[9][n + 1][m + 1];
double rho[n + 1][m + 1];
double u[n + 1][m + 1];
double v[n + 1][m + 1];
double w[9] = { 4.0 / 9.0, 1.0 / 9.0, 1.0 / 9.0, 1.0 / 9.0, 1.0 / 9.0, 1.0 / 36.0, 1.0 / 36.0, 1.0 / 36.0, 1.0 / 36.0 };
double cx[9] = { 0,1,0,-1,0,1,-1,-1,1 };
double cy[9] = { 0,0,1,0,-1,1,1,-1,-1 };
double g[9][n + 1][m + 1];
double geq[9][n + 1][m + 1];
double th[n + 1][m + 1];

double u0 = 0.2;
double sumvel0 = 0.0;
double rho0 = 5.0;
double dx = 1.0;
double dy = 1.0;
double dt = 1.0;
double tw = 1.0;
double visco = 0.02;
double pr = 0.71;
double alpha = visco/pr;
double omega = 1.0 / (3.0*visco + 0.5);
double omegat = 1.0 / (3.0*alpha + 0.5);


void init()
{
	omp_set_num_threads(NUM_THREADS);
	# pragma omp parallel for shared(rho, n, m, rho0, th, g, u, v)
	for (int j = 0; j < m + 1; j++)
	{
		for (int i = 0; i < n + 1; i++)
		{
			th[i][j] = 0.0;
			rho[i][j] = rho0;
            u[i][j] = 0.0;
            v[i][j] = 0.0;
            for (int k =  0; k < 9; k++)
                g[k][i][j] = 0.0;
		}
	}

	# pragma omp parallel for shared(u, n, m, v, u0)
	for (int i = 1; i < n; i++)
	{
		u[i][m] = u0;
		v[i][m] = 0.0;
	}
}


void collesion()
{
	omp_set_num_threads(NUM_THREADS);
	# pragma omp parallel for shared(u, v, cx, cy, rho, w, omega, feq, f, n, m)
	for (int i = 0; i < n + 1; i++)
	{
		for (int j = 0; j < m + 1; j++)
		{
			double t1 = u[i][j] * u[i][j] + v[i][j] * v[i][j];
			for (int k = 0; k < 9; k++)
			{
				double t2 = u[i][j] * cx[k] + v[i][j] * cy[k];
				feq[k][i][j] = rho[i][j] * w[k] * (1.0 + 3.0*t2 + 4.5*t2*t2 - 1.5*t1);
				f[k][i][j] = omega * feq[k][i][j] + (1.0 - omega)*f[k][i][j];
			}
		}
	}
}


void collt()
{
    omp_set_num_threads(NUM_THREADS);
    # pragma omp parallel for shared(u, v, cx, cy, w, omegat, geq, g, n, m, th)
    for (int i = 0; i < n + 1; i++)
	{
		for (int j = 0; j < m + 1; j++)
		{
			for (int k = 0; k < 9; k++)
			{
				geq[k][i][j] = th[i][j] * w[k] * (1.0 + 3.0*(u[i][j]*cx[k] + v[i][j]*cy[k]));
				g[k][i][j] = omegat * geq[k][i][j] + (1.0 - omegat)*g[k][i][j];
			}
		}
	}
}


void streaming()
{
	omp_set_num_threads(NUM_THREADS);
	# pragma omp parallel for shared(f, n, m)
	for (int j = 0; j < m + 1; j++)
	{
		/// right to left
		for (int i = n; i > 0; i--)
		{
			f[1][i][j] = f[1][i - 1][j];
		}

		/// left to right
		for (int i = 0; i < n; i++)
		{
			f[3][i][j] = f[3][i + 1][j];
		}
	}

	# pragma omp parallel for shared(f, n, m)
	/// top to bottom
	for (int j = m; j > 0; j--)
	{
		for (int i = 0; i < n+1; i++)
		{
			f[2][i][j] = f[2][i][j - 1];
		}
		for (int i = n; i > 0; i--)
		{
			f[5][i][j] = f[5][i - 1][j - 1];
		}
		for (int i = 0; i < n; i++)
		{
			f[6][i][j] = f[6][i + 1][j - 1];
		}
	}

	# pragma omp parallel for shared(f, n, m)
	/// bottom to top
	for (int j = 0; j < m; j++)
	{
		for (int i = 0; i < n + 1; i++)
		{
			f[4][i][j] = f[4][i][j + 1];
		}
		for (int i = 0; i < n; i++)
		{
			f[7][i][j] = f[7][i + 1][j + 1];
		}
		for (int i = n; i > 0; i--)
		{
			f[8][i][j] = f[8][i - 1][j + 1];
		}
	}
}


void sfbound()
{
	omp_set_num_threads(NUM_THREADS);
	# pragma omp parallel for shared(f, m, u0)
	
	for (int j = 0; j < m + 1; j++)
	{
		/// bounce back on west boundary
		f[1][0][j] = f[3][0][j];
		f[5][0][j] = f[7][0][j];
		f[8][0][j] = f[6][0][j];

		/// bounce back on east boundary
		f[3][n][j] = f[1][n][j];
		f[7][n][j] = f[5][n][j];
		f[6][n][j] = f[8][n][j];
	}
	
	# pragma omp parallel for shared(f, n)
	/// bounce back on south boundary
	for (int i = 0; i < n + 1; i++)
	{
		f[2][i][0] = f[4][i][0];
		f[5][i][0] = f[7][i][0];
		f[6][i][0] = f[8][i][0];
	}

	# pragma omp parallel for shared(f, m, n)
	/// moving lid, north boundary
	for (int i = 1; i < n; i++)
	{
		double rhon = f[0][i][m] + f[1][i][m] + f[3][i][m] + 2.0 * (f[2][i][m] + f[6][i][m] + f[5][i][m]);
		f[4][i][m] = f[2][i][m];
		f[7][i][m] = f[5][i][m] - rhon * u0 / 6.0;
		f[8][i][m] = f[6][i][m] + rhon * u0 / 6.0;
	}
}


void gbound()
{
    omp_set_num_threads(NUM_THREADS);
	# pragma omp parallel for shared(g, m, n)
    /// west boundary condition, T = 0
    for (int j = 0; j < m + 1; j++)
    {
        g[1][0][j] = -g[3][0][j];
		g[5][0][j] = -g[7][0][j];
		g[8][0][j] = -g[6][0][j];
    }

    /// east boundary condition, T = 0
    # pragma omp parallel for shared(g, m, n)
    for (int j = 0; j < m + 1; j++)
    {
        g[6][n][j] = -g[8][n][j];
        g[3][n][j] = -g[1][n][j];
        g[7][n][j] = -g[5][n][j];
        g[2][n][j] = -g[4][n][j];
        g[0][n][j] = 0.0;
    }

    /// top boundary condition, T = tw = 1.0
    # pragma omp parallel for shared(g, m, n, tw, w)
    for (int i = 0; i < n + 1; i++)
    {
        g[8][i][m] = tw * (w[8] + w[6]) - g[6][i][m];
        g[7][i][m] = tw * (w[7] + w[5]) - g[5][i][m];
        g[4][i][m] = tw * (w[4] + w[2]) - g[2][i][m];
        g[1][i][m] = tw * (w[2] + w[3]) - g[3][i][m];
    }

    /// bottom boundary condition, Adiabatic
    # pragma omp parallel for shared(g, m, n)
    for (int i = 0; i < n + 1; i++)
    {
        g[1][i][0] = g[1][i][1];
        g[2][i][0] = g[2][i][1];
        g[3][i][0] = g[3][i][1];
        g[4][i][0] = g[4][i][1];
        g[5][i][0] = g[5][i][1];
        g[6][i][0] = g[6][i][1];
        g[7][i][0] = g[7][i][1];
        g[8][i][0] = g[8][i][1];
    }
}


void tcalcu()
{
    omp_set_num_threads(NUM_THREADS);
	# pragma omp parallel for shared(th, m, n, g)
    for (int j = 1; j < m; j++)
    {
        for (int i = 1; i < n; i++)
        {
            double ssumt = 0.0;
            for (int k = 0; k < 9; k++)
            {
                ssumt += g[k][i][j];
            }
            th[i][j] = ssumt;
        }
    }
}


void rhouv()
{
	omp_set_num_threads(NUM_THREADS);
	# pragma omp parallel for shared(f, m, n, rho)
	for (int j = 0; j < m + 1; j++)
	{
		for (int i = 0; i < n + 1; i++)
		{
			double ssum = 0.0;
			for (int k = 0; k < 9; k++)
				ssum += f[k][i][j];
			rho[i][j] = ssum;
		}
	}

	# pragma omp parallel for shared(f, m, u0)
	for (int j = 1; j < m; j++)
	{
		for (int i = 1; i < n + 1; i++)
		{
			double usum = 0.0;
			double vsum = 0.0;
			for (int k = 0; k < 9; k++)
			{
				usum += f[k][i][j] * cx[k];
				vsum += f[k][i][j] * cy[k];
			}
			u[i][j] = usum / rho[i][j];
			v[i][j] = vsum / rho[i][j];
		}
	}
}


void output()
{
	ofstream outfile;

	/// user define filename and title
	string filename = "rho.dat";
	string title = "2D-velocity-temperature";

	/// start write data into tecplot dat format file
	outfile.open(filename);
	outfile << "TITLE = \"" << title << "\"" << endl;
	outfile << "VARIABLES = \"X\", \"Y\", \"Z\", \"U\", \"V\", \"T\"" << endl;
	outfile << "ZONE I = " << n << ", J = " << m << ", K = 1, F = point" << endl;
	for (int j = m; j >= 0; j--)
	{
		for (int i = 0; i < n + 1; i++)
		{
			outfile << i << "," << j << "," << 0 << "," << u[i][j] << "," << v[i][j] << th[i][j] << endl;
		}
	}
	outfile.close();
}

int main()
{
    init();
    for (int kk = 1; kk <= mstep; kk++)
    {
        collesion();
        streaming();
        sfbound();
        rhouv();

        omp_set_num_threads(NUM_THREADS);
	    # pragma omp parallel for shared(g, m, n)
        for (int j = 0; j < m + 1; j++)
        {
            for (int i = 0; i < n + 1; i++)
            {
                double sum = 0.0;
                for (int k = 0; k < 9; k++)
                    sum += g[k][i][j];
                th[i][j] = sum;
            }
        }

        collt();    /// collestion for scalar
        streaming();    /// streaming for scalar
        gbound();
    }
    output();
    return 0;
}