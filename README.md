# DICOM Viewer

Visualizador de imagens DICOM para desktop, escrito em **C++23** com **Qt 6 (Widgets)** e **DCMTK**.

O usuário seleciona um arquivo DICOM no disco e a aplicação exibe a imagem respeitando dimensões,
escala de cinza, orientação e a transformação de VOI definida no próprio arquivo, com ajuste interativo
de janela (Window Level / Window Width), painel de metadados e suporte a imagens coloridas.

![Tela principal](docs/screenshots/main.png)

## Funcionalidades

**Escopo obrigatório**

- Seleção de arquivo por diálogo nativo (`Ctrl+O`), por arrastar-e-soltar ou por argumento de linha de comando.
- Leitura de arquivos DICOM Part 10, comprimidos ou não (ver tabela de sintaxes de transferência).
- Exibição correta de imagens monocromáticas:
  - `MONOCHROME1` e `MONOCHROME2`;
  - 8 a 32 bits alocados, com ou sem sinal;
  - Modality LUT / Rescale Slope e Intercept;
  - VOI LUT Function `LINEAR`, `LINEAR_EXACT` e `SIGMOID` (DICOM PS3.3 C.11.2.1);
  - Pixel Padding Value / Range exibido em preto;
  - proporção de pixel não quadrado (Pixel Spacing / Pixel Aspect Ratio).
- Orientação: a imagem é exibida exatamente como armazenada (linha 0 no topo, coluna 0 à esquerda) e os
  marcadores de orientação do paciente (A/P, R/L, H/F) são desenhados nas bordas, a partir de
  Patient Orientation (0020,0020) ou Image Orientation (Patient) (0020,0037).
- Mensagens de erro específicas para arquivo inexistente, ilegível, não DICOM, DICOM sem imagem,
  compressão não suportada e dados de pixel corrompidos. Um erro nunca derruba a aplicação nem descarta a
  imagem que já estava aberta.

**Diferenciais**

- **Window Level / Window Width** interativo (arrastar com o botão esquerdo), campos numéricos e presets
  lidos do arquivo (Window Center/Width e respectivas explicações).
- **Metadados DICOM** em árvore (sequências aninhadas), com filtro por tag, nome ou valor.
- **Imagens coloridas**: `RGB` (planar ou intercalado), `YBR_FULL`, `YBR_FULL_422` e `PALETTE COLOR`.

**Usabilidade**

- Zoom ancorado no cursor (roda do mouse), pan, ajustar à janela e 100%.
- Carregamento em segundo plano: a interface nunca trava, mesmo com arquivos grandes.
- Pirâmide de resoluções: o ajuste de janela continua fluido em imagens de vários megapixels.
- Valor do pixel sob o cursor na barra de status.

### Sintaxes de transferência suportadas

| Sintaxe de transferência | Suporte |
|---|---|
| Implicit VR Little Endian, Explicit VR Little/Big Endian, Deflated | Sim |
| JPEG Baseline, Extended e Lossless (Process 1, 2/4, 14, 14 SV1) | Sim |
| JPEG-LS Lossless e Near-Lossless | Sim |
| RLE Lossless | Sim |
| JPEG 2000, HTJ2K, JPEG XL, MPEG, HEVC | Não: a aplicação informa qual é a sintaxe não suportada |

## Bibliotecas utilizadas

| Biblioteca | Versão | Uso |
|---|---|---|
| [Qt](https://www.qt.io/) | **6.11.3** | Interface gráfica (módulos Core, Gui, Widgets, Concurrent) e testes (Test) |
| [DCMTK](https://dicom.offis.de/dcmtk) | 3.7.1 | Leitura de DICOM, descompressão e transformações de imagem (`dcmdata`, `dcmimgle`, `dcmimage`, `dcmjpeg`, `dcmjpls`) |

O DCMTK é obtido e compilado automaticamente pelo
[vcpkg](https://vcpkg.io/) em modo manifesto (`vcpkg.json`), com versão fixada por `builtin-baseline`.
O Qt é instalado pelo instalador oficial.

**Versão do Qt utilizada: 6.11.3** (kit `msvc2022_64`). A versão mínima aceita pelo build é a 6.8.

## Requisitos

| Ferramenta | Versão mínima | Observação |
|---|---|---|
| Compilador C++23 | MSVC 19.38 (Visual Studio 2022 17.8), GCC 13 ou Clang 17 | O projeto usa `std::expected` |
| CMake | 3.25 | |
| Qt | 6.8 (desenvolvido e testado com a 6.11.3) | Componentes: Desktop (`msvc2022_64` no Windows / `gcc_64` no Linux) |
| vcpkg | qualquer versão recente | Variável de ambiente `VCPKG_ROOT` apontando para a instalação |
| Git | | |

Plataforma de desenvolvimento e teste: **Windows 11 x64**. O código não usa nenhuma API específica de
plataforma e o build está preparado para Linux (instruções abaixo).

## Configuração

### Windows

1. Instale o **Visual Studio 2022** (ou mais novo) com a carga de trabalho *Desenvolvimento para desktop com C++*.
2. Instale o **Qt 6.11.3** para MSVC 2022 64-bit pelo [instalador oficial](https://www.qt.io/download-qt-installer-oss)
   (destino padrão: `C:\Qt`).
3. Instale o **vcpkg**:

   ```powershell
   git clone https://github.com/microsoft/vcpkg C:\vcpkg
   C:\vcpkg\bootstrap-vcpkg.bat -disableMetrics
   ```

4. Defina as duas variáveis de ambiente usadas pelos presets do CMake (abra um novo terminal depois):

   ```powershell
   setx VCPKG_ROOT "C:\vcpkg"
   setx QT_ROOT_DIR "C:\Qt\6.11.3\msvc2022_64"
   ```

### Linux (Ubuntu 24.04 ou mais novo)

```bash
sudo apt install build-essential cmake ninja-build git curl zip unzip tar pkg-config \
                 libgl1-mesa-dev libxkbcommon-dev libfontconfig1-dev
git clone https://github.com/microsoft/vcpkg ~/vcpkg && ~/vcpkg/bootstrap-vcpkg.sh -disableMetrics
export VCPKG_ROOT=~/vcpkg
export QT_ROOT_DIR=~/Qt/6.11.3/gcc_64   # Qt instalado pelo instalador oficial
```

## Compilação

Os comandos são os mesmos em todas as plataformas; mude apenas o nome do preset
(`windows` ou `linux`). Na primeira configuração o vcpkg compila o DCMTK, o que leva alguns minutos.

```powershell
git clone <URL-do-repositório> dicom-viewer
cd dicom-viewer
cmake --preset windows
cmake --build --preset windows
```

## Execução

```powershell
.\build\windows\src\app\Release\dicom-viewer.exe
.\build\windows\src\app\Release\dicom-viewer.exe samples\anonymized_mamo.dcm
```

No Windows, as DLLs do Qt e do DCMTK são copiadas automaticamente para a pasta do executável durante o
build (`windeployqt` + vcpkg), então ele pode ser iniciado direto pelo Explorer.

No Linux: `./build/linux/src/app/dicom-viewer samples/anonymized_mamo.dcm`.

## Testes

```powershell
ctest --preset windows
```

A suíte usa Qt Test e cobre o núcleo de processamento (VOI LUT, pirâmide de resoluções, orientação),
o carregamento de todos os arquivos de `samples/` (incluindo os inválidos) e os componentes de interface.
Os valores esperados de pixel foram obtidos com uma implementação independente (pydicom).

## Como usar

| Ação | Como |
|---|---|
| Abrir arquivo | `Ctrl+O`, botão **Abrir** ou arrastar o arquivo para a janela |
| Ajustar janela (WL/WW) | Arrastar com o botão esquerdo: horizontal altera a largura, vertical altera o centro |
| Escolher preset de janela | Lista **Janela** na barra de ferramentas |
| Restaurar janela original | `R` |
| Zoom | Roda do mouse (ancorado no cursor), `Ctrl++`, `Ctrl+-` |
| Ajustar à janela / tamanho real | `Ctrl+0` ou duplo clique / `Ctrl+1` |
| Mover a imagem (pan) | Arrastar com o botão do meio, ou `Ctrl` + arrastar com o esquerdo |
| Mostrar/ocultar metadados | `F9` |

Em imagens coloridas não há janelamento; arrastar com o botão esquerdo move a imagem.

## Arquitetura

O código é dividido em três camadas com dependência em um único sentido:

```
app  (Qt Widgets)  ──►  dicom  (DCMTK)  ──►  core  (C++23 puro)
```

| Camada | Alvo CMake | Depende de | Responsabilidade |
|---|---|---|---|
| `src/core` | `dv_core` | Biblioteca padrão | Modelo de domínio (`ImageDocument`), funções de VOI, LUT de tons de cinza, pirâmide de resoluções, rótulos de orientação. Sem Qt e sem DCMTK. |
| `src/dicom` | `dv_dicom` | `dv_core`, DCMTK | Único ponto de contato com o DCMTK: lê o arquivo, descomprime, aplica a Modality LUT e devolve um `ImageDocument` ou um `LoadError`. |
| `src/app` | `dicom-viewer` | `dv_dicom`, Qt | Janela principal, viewport, controles de janela, painel de metadados e carregamento assíncrono. |

Decisões principais:

- **DCMTK isolado atrás de uma função.** `dv::loadDicomFile(path)` devolve
  `std::expected<ImageDocument, LoadError>`. Nenhum tipo do DCMTK aparece nos cabeçalhos públicos, e nenhuma
  exceção atravessa a fronteira: toda falha vira um `LoadError` com código e detalhe técnico.
- **Pipeline de exibição explícito.** O DCMTK decodifica e aplica a Modality LUT; os valores resultantes
  ficam em um buffer de 16 bits. A VOI LUT é calculada pela aplicação como uma tabela de até 65 536 entradas,
  então mudar a janela custa uma consulta de tabela por pixel, sem reprocessar o arquivo.

  ```
  Pixel Data ─► descompressão ─► Modality LUT ─► [buffer 16 bits] ─► VOI LUT ─► inversão (MONOCHROME1) ─► tela
               └───────────── DCMTK ──────────┘                     └──────────── core ─────────────┘
  ```

- **Pirâmide de resoluções.** No carregamento são gerados níveis com metade da resolução (média 2×2). O
  viewport renderiza o nível mais próximo do zoom atual, o que elimina aliasing ao reduzir a imagem e mantém
  o ajuste de janela instantâneo em imagens grandes (a mamografia de exemplo tem 6,8 megapixels).
- **Carregamento fora da thread de interface** com `QtConcurrent` e `QFuture::then`. Resultados de cargas
  obsoletas são descartados por um contador de geração.
- **Documento imutável.** O `ImageDocument` é compartilhado como `std::shared_ptr<const ImageDocument>`
  entre a thread de carga e a interface, sem locks.

### Estrutura de pastas

```
dicom-viewer/
├── CMakeLists.txt            Projeto raiz
├── CMakePresets.json         Presets de configuração, build e teste (windows, linux)
├── vcpkg.json                Dependências (DCMTK) com versão fixada
├── cmake/                    Módulos CMake (avisos do compilador, deploy do Qt)
├── src/
│   ├── core/                 Domínio e processamento de imagem (C++ puro)
│   ├── dicom/                Adaptador DCMTK
│   └── app/                  Interface Qt Widgets
├── tests/
│   ├── core/                 Testes unitários do núcleo
│   ├── dicom/                Testes de carregamento com os arquivos de samples/
│   ├── app/                  Testes dos componentes de interface
│   └── support/              Utilitários compartilhados pelos testes
├── samples/                  Arquivos DICOM de exemplo (ver samples/README.md)
└── docs/screenshots/         Imagens usadas neste README
```

## Tratamento de erros

| Situação | Mensagem apresentada |
|---|---|
| Arquivo não existe | "Arquivo não encontrado." |
| Sem permissão de leitura, ou o caminho é uma pasta | "Não foi possível ler o arquivo." |
| Não é DICOM (ou está vazio) | "O arquivo selecionado não é um arquivo DICOM válido." |
| DICOM sem Pixel Data (laudo estruturado, DICOMDIR etc.) | "O arquivo é DICOM, mas não contém uma imagem." |
| Compressão sem decodificador | "A imagem usa uma compressão não suportada" + nome da sintaxe de transferência |
| Atributos de imagem inválidos ou não suportados | "O formato desta imagem não é suportado." |
| Arquivo DICOM truncado ou Pixel Data corrompido | "Os dados da imagem estão corrompidos ou incompletos." |
| Memória insuficiente | "Memória insuficiente para carregar a imagem." |

O detalhe técnico devolvido pelo DCMTK fica disponível no botão **Mostrar detalhes** do diálogo.

## Limitações conhecidas

- Arquivos multi-frame: apenas o primeiro quadro é exibido (a barra de status informa o total de quadros).
- A VOI LUT Sequence (0028,3010) não é aplicada; quando o arquivo traz Window Center/Width, eles são usados.
- Overlays (grupo 60xx) e Presentation States não são desenhados.
- Imagens com faixa dinâmica maior que 16 bits após a Modality LUT são quantizadas para 65 536 níveis.
