#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mge::bench {

    // Evita que el compilador optimice y elimine variables no leídas
    template <typename T> inline void doNotOptimize(const T& val) {
#if defined(__clang__) || defined(__GNUC__)
        asm volatile("" : : "r,m"(val) : "memory");
#else
        volatile const void* p = &val;
        (void)p;
#endif
    }

    template <typename T> inline void doNotOptimize(T& val) {
#if defined(__clang__) || defined(__GNUC__)
        asm volatile("" : "+r,m"(val) : : "memory");
#else
        volatile void* p = &val;
        (void)p;
#endif
    }

    struct BenchResult {
        std::string name;
        double medianNs = 0.0;
        double minNs = 0.0;
        std::uint64_t iterations = 0;
        std::string unit;
        bool suspicious = false;
    };

    class BenchRunner {
    public:
        BenchRunner() = default;

        BenchRunner(int argc, char** argv) { parseCli(argc, argv); }

        void parseCli(int argc, char** argv) {
            for (int i = 1; i < argc; ++i) {
                const std::string_view arg = argv[i];
                if (arg == "--json" && i + 1 < argc) {
                    m_jsonPath = argv[++i];
                } else if (arg == "--filter" && i + 1 < argc) {
                    m_filter = argv[++i];
                } else if (arg == "--quick") {
                    m_quick = true;
                } else if (arg == "--help" || arg == "-h") {
                    printHelp(argv[0]);
                    std::exit(0);
                }
            }
        }

        bool isQuick() const { return m_quick; }
        const std::string& jsonPath() const { return m_jsonPath; }
        const std::string& filter() const { return m_filter; }

        void add(std::string name, std::string unitLabel, std::uint64_t itemsPerRun,
                 std::function<void()> fn) {
            if (!m_filter.empty() && name.find(m_filter) == std::string::npos) {
                return;
            }
            m_benchmarks.push_back({std::move(name), std::move(unitLabel), itemsPerRun, std::move(fn)});
        }

        void run(int repeats = 7) {
            m_results.clear();
            for (auto& b : m_benchmarks) {
                // Warm-up run
                b.fn();

                std::vector<double> samples;
                samples.reserve(static_cast<std::size_t>(repeats));
                for (int r = 0; r < repeats; ++r) {
                    const auto start = std::chrono::steady_clock::now();
                    b.fn();
                    const auto end = std::chrono::steady_clock::now();
                    const double elapsedNs = std::chrono::duration<double, std::nano>(end - start).count();
                    const double perItemNs =
                        (b.itemsPerRun > 0) ? (elapsedNs / static_cast<double>(b.itemsPerRun)) : elapsedNs;
                    samples.push_back(perItemNs);
                }

                std::sort(samples.begin(), samples.end());
                const double minNs = samples.front();
                double medianNs = 0.0;
                if (samples.size() % 2 == 1) {
                    medianNs = samples[samples.size() / 2];
                } else {
                    medianNs = 0.5 * (samples[samples.size() / 2 - 1] + samples[samples.size() / 2]);
                }

                const bool suspicious = (medianNs < 1.0);
                m_results.push_back({b.name, medianNs, minNs, b.itemsPerRun, b.unitLabel, suspicious});
            }
        }

        void printTable() const {
            if (m_results.empty()) {
                std::printf("No se ejecutaron benchmarks.\n");
                return;
            }
            std::printf("\n=================================================================================="
                        "===============\n");
            std::printf("%-42s %14s %14s %12s  %-12s\n", "Benchmark", "Median (ns)", "Min (ns)", "Iterations",
                        "Unit");
            std::printf("------------------------------------------------------------------------------------"
                        "-------------\n");
            for (const auto& r : m_results) {
                if (r.suspicious) {
                    std::printf("%-42s %14.2f %14.2f %12llu  %-12s  ⚠️  suspiciously fast\n", r.name.c_str(),
                                r.medianNs, r.minNs, static_cast<unsigned long long>(r.iterations),
                                r.unit.c_str());
                } else {
                    std::printf("%-42s %14.2f %14.2f %12llu  %-12s\n", r.name.c_str(), r.medianNs, r.minNs,
                                static_cast<unsigned long long>(r.iterations), r.unit.c_str());
                }
            }
            std::printf("===================================================================================="
                        "=============\n\n");
        }

        bool writeJson(const std::string& path) const {
            if (path.empty()) {
                return false;
            }
            std::ofstream out(path);
            if (!out.is_open()) {
                std::fprintf(stderr, "Error: no se pudo abrir '%s' para escribir JSON de benchmarks\n",
                             path.c_str());
                return false;
            }
            out << "{\n  \"benchmarks\": [\n";
            for (std::size_t i = 0; i < m_results.size(); ++i) {
                const auto& r = m_results[i];
                out << "    {\n";
                out << "      \"name\": \"" << escapeJson(r.name) << "\",\n";
                out << "      \"medianNs\": " << std::fixed << std::setprecision(2) << r.medianNs << ",\n";
                out << "      \"minNs\": " << std::fixed << std::setprecision(2) << r.minNs << ",\n";
                out << "      \"iterations\": " << r.iterations << ",\n";
                out << "      \"unit\": \"" << escapeJson(r.unit) << "\",\n";
                out << "      \"suspicious\": " << (r.suspicious ? "true" : "false") << "\n";
                out << "    }" << (i + 1 < m_results.size() ? "," : "") << "\n";
            }
            out << "  ]\n}\n";
            return true;
        }

        const std::vector<BenchResult>& results() const { return m_results; }

    private:
        struct BenchmarkItem {
            std::string name;
            std::string unitLabel;
            std::uint64_t itemsPerRun;
            std::function<void()> fn;
        };

        static std::string escapeJson(std::string_view s) {
            std::string res;
            for (char c : s) {
                if (c == '"') {
                    res += "\\\"";
                } else if (c == '\\') {
                    res += "\\\\";
                } else if (c == '\n') {
                    res += "\\n";
                } else {
                    res += c;
                }
            }
            return res;
        }

        static void printHelp(const char* prog) {
            std::printf("Uso: %s [opciones]\n", prog);
            std::printf("Opciones:\n");
            std::printf("  --json <path>       Guarda los resultados en formato JSON\n");
            std::printf("  --filter <substr>   Ejecuta solo benchmarks cuyo nombre contiene <substr>\n");
            std::printf("  --quick             Modo rapido con menos iteraciones (para smoke CI)\n");
            std::printf("  --help, -h          Muestra esta ayuda\n");
        }

        std::string m_jsonPath;
        std::string m_filter;
        bool m_quick = false;
        std::vector<BenchmarkItem> m_benchmarks;
        std::vector<BenchResult> m_results;
    };

} // namespace mge::bench
