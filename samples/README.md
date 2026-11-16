# Arquivos DICOM de exemplo

Usados pelos testes (`tests/`) e para validação manual. Nenhum contém dados reais de paciente.

| Arquivo | Origem | O que exercita |
|---|---|---|
| `anonymized_mamo.dcm` | Enunciado do teste | Mamografia 2394×2850, 12 bits, MONOCHROME2, JPEG Lossless (1.2.840.10008.1.2.4.70), VOI LUT Function `SIGMOID`, 3 presets de janela, Pixel Padding, Patient Orientation `A\FR` |
| `CT_small.dcm` | pydicom | 16 bits com sinal, Rescale Intercept −1024, sem janela no arquivo |
| `MR_small.dcm` | pydicom | 16 bits, janela linear C 600 / L 1600 |
| `MR_small_implicit.dcm` | pydicom | Implicit VR Little Endian |
| `MR_small_bigendian.dcm` | pydicom | Explicit VR Big Endian |
| `MR_small_RLE.dcm` | pydicom | RLE Lossless |
| `MR_small_jpeg_ls_lossless.dcm` | pydicom | JPEG-LS Lossless |
| `image_dfl.dcm` | pydicom | Deflated Explicit VR Little Endian, 512×512, 8 bits |
| `MR_small_jp2klossless.dcm` | pydicom | JPEG 2000: erro de compressão não suportada |
| `JPGExtended.dcm` | pydicom | JPEG Extended (12 bits, com perdas) |
| `examples_overlay.dcm` | pydicom | 12 bits, 2 presets de janela |
| `rtdose_1frame.dcm` | pydicom | 32 bits (faixa dinâmica > 16 bits) |
| `examples_rgb_color.dcm` | pydicom | RGB 8 bits sem compressão |
| `SC_rgb_small_odd.dcm` | pydicom | RGB 3×3: largura ímpar, valida o alinhamento de linha |
| `ExplVR_BigEnd.dcm` | pydicom | RGB com Planar Configuration = 1, Big Endian |
| `SC_rgb_jpeg_gdcm.dcm` | pydicom | RGB em JPEG Lossless |
| `SC_ybr422_jpeg.dcm` | pydicom (`SC_rgb_dcmtk_+eb+cy+np.dcm`) | YBR_FULL_422 em JPEG Baseline |
| `SC_ybr_full_422_uncompressed.dcm` | pydicom | YBR_FULL_422 sem compressão |
| `examples_palette.dcm` | pydicom | PALETTE COLOR |
| `examples_ybr_color.dcm` | pydicom | Multi-frame (30 quadros), YBR_FULL_422 JPEG |
| `synthetic_monochrome1.dcm` | Derivado de `MR_small.dcm` | MONOCHROME1 (escala invertida) |
| `synthetic_nonsquare_latin1.dcm` | Derivado de `MR_small.dcm` | Pixel Spacing `2\1` (pixel não quadrado) e nome com acentos em ISO_IR 100 |
| `MR_truncated.dcm` | pydicom | Pixel Data truncado |
| `test-SR.dcm` | pydicom | DICOM válido sem imagem (Structured Report) |
| `not_dicom.dcm` | Sintético | Arquivo de texto com extensão `.dcm` |
| `empty.dcm` | Sintético | Arquivo vazio |

Origem "pydicom": arquivos de teste do [pydicom](https://github.com/pydicom/pydicom)
(`src/pydicom/data/test_files`, licença MIT).
