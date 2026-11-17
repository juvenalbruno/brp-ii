// verificar_resultados.cpp — relê os JSONs de resultado já gravados e confere que cada
// sequência é executável na instância correspondente: respeita H, só move topos, só
// move blocos acima do alvo (A1), esvazia a baia, e o número de movimentos é igual a
// "relocacoes". Confere também que "sequencia" (pares [origem, destino]) descreve os
// mesmos movimentos.
//
// Uso: ./verificar_resultados <arquivo ou pasta de instâncias> <pasta_de_resultados> [...]
// O resultado <pasta>/<chave>.json corresponde à instância de mesma chave
// (ex.: brp_W3_H4_N8_id01), localizada pelos campos "arquivo" e "id" do resultado.
#include <map>

#include "../src/instance.hpp"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "uso: verificar_resultados <arquivo ou pasta de instâncias> <pasta_de_resultados> [...]\n";
        return 2;
    }
    // Carrega todas as instâncias, indexadas pela chave.
    std::map<std::string, brp::Instancia> instancias;
    try {
        for (const brp::fs::path& arq : brp::listarInstancias(argv[1]))
            for (brp::Instancia& inst : brp::lerInstancias(arq)) {
                std::string chave = inst.chave;
                instancias.emplace(std::move(chave), std::move(inst));
            }
    } catch (const std::exception& e) {
        std::cerr << "ERRO: " << e.what() << "\n";
        return 2;
    }

    int conferidos = 0, semSolucao = 0, falhas = 0;
    for (int a = 2; a < argc; ++a) {
        std::vector<brp::fs::path> arquivos;
        try {
            arquivos = brp::listarInstancias(argv[a]);
        } catch (const std::exception& e) {
            std::cerr << "ERRO: " << e.what() << "\n";
            ++falhas;
            continue;
        }
        for (const brp::fs::path& arq : arquivos) {
            try {
                brp::Json res = brp::lerJson(arq);
                auto it = instancias.find(arq.stem().string());
                if (it == instancias.end()) throw std::runtime_error("instância correspondente não encontrada");
                const brp::Instancia& inst = it->second;

                const brp::Json* arquivo = res.campo("arquivo");
                const brp::Json* id = res.campo("id");
                if (!arquivo || arquivo->texto != inst.arquivo) throw std::runtime_error("campo \"arquivo\" não confere");
                int idRes = (id && brp::ehInteiro(*id)) ? static_cast<int>(id->numero) : 0;
                if (idRes != inst.id) throw std::runtime_error("campo \"id\" não confere");

                const brp::Json* rel = res.campo("relocacoes");
                const brp::Json* lista = res.campo("movimentos");
                const brp::Json* seq = res.campo("sequencia");
                if (!rel || !lista || lista->tipo != brp::Json::Lista || !seq || seq->tipo != brp::Json::Lista)
                    throw std::runtime_error("campos \"relocacoes\"/\"movimentos\"/\"sequencia\" ausentes");
                if (rel->tipo == brp::Json::Nulo) {  // método não encontrou solução
                    ++semSolucao;
                    continue;
                }
                std::vector<brp::Movimento> movs;
                for (const brp::Json& jm : lista->lista) {
                    auto inteiro = [&](const char* chave) {
                        const brp::Json* c = jm.campo(chave);
                        if (!c || !brp::ehInteiro(*c)) throw std::runtime_error(std::string("movimento sem ") + chave);
                        return static_cast<int>(c->numero);
                    };
                    movs.push_back({inteiro("bloco"), inteiro("origem"), inteiro("destino"), inteiro("periodo")});
                }
                if (static_cast<double>(movs.size()) != rel->numero)
                    throw std::runtime_error("\"relocacoes\" difere do número de movimentos");
                if (seq->lista.size() != movs.size()) throw std::runtime_error("\"sequencia\" e \"movimentos\" têm tamanhos diferentes");
                for (std::size_t i = 0; i < movs.size(); ++i) {
                    const brp::Json& par = seq->lista[i];
                    if (par.tipo != brp::Json::Lista || par.lista.size() != 2 || par.lista[0].numero != movs[i].origem ||
                        par.lista[1].numero != movs[i].destino)
                        throw std::runtime_error("\"sequencia\" difere de \"movimentos\" na posição " + std::to_string(i + 1));
                }
                std::string erro;
                if (!brp::verificarSequencia(inst, movs, erro)) throw std::runtime_error(erro);
                ++conferidos;
            } catch (const std::exception& e) {
                std::cerr << "FALHA em " << arq.string() << ": " << e.what() << "\n";
                ++falhas;
            }
        }
    }
    std::printf("sequências conferidas: %d  sem solução: %d  falhas: %d\n", conferidos, semSolucao, falhas);
    return falhas ? 1 : 0;
}
