#include <iostream>
#include <vector>
#include <algorithm>

void merge_sort(std::vector <int> &A, int l, int r, int d)
{
    if (r - l <= 1) {
        return;
    }
    int m = (l + r) / 2;

    if (d > 0) {
        #pragma omp parallel sections num_threads(2)
        {
            #pragma omp section 
            {
                merge_sort(A, l, m, d - 1);
            }
            #pragma omp section
            {
                merge_sort(A, m, r, d - 1);
            }
        }
    } else {
        merge_sort(A, l, m, d);
        merge_sort(A, m, r, d);
    }
    
    std::vector <int> res(r - l);
    std::merge(A.begin() + l, A.begin() + m, A.begin() + m, A.begin() + r, res.begin());
    std::copy(res.begin(), res.end(), A.begin() + l);
}

int main()
{
    std::ios::sync_with_stdio(0);
    std::cin.tie(NULL);

    int n;
    std::cin >> n;
    std::vector <int> A(n);

    for (int i = 0; i < n; i++) {
        std::cin >> A[i];
    }
    merge_sort(A, 0, n, 2);

    for (int i = 0; i < n; i++) {
        std::cout << A[i] << ' ';
    }
    std::cout << std::endl;
    return 0;
}
