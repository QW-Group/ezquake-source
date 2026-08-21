# Onde paramos — Vulkan renderer / SDL3 port

Atualizado em: 2026-08-07 (sessão Claude, Windows, `E:\Projetos Linux\ezquake-sdl3-vulkan-pr`) — ver seção "Sessão 2026-08-06/07" logo abaixo para o estado mais recente. As demais seções continuam válidas como histórico.

## Sessão 2026-08-06/07 (Claude, Windows — drawflat Vulkan corrigido, buffers dinâmicos em heap rápido, VkQueryPool no timerefresh)

Commit `88d916ba` enviado para `origin/feature/sdl3-vulkan-pr`. Build Release limpo gerado (`_build_release.bat`, config `Release`, sem debug info) e deployado em `C:\ezquake\ezquake.exe`.

- **Bug de cor no drawflat (Vulkan) corrigido de verdade.** `r_wallcolor`/`r_floorcolor` não apareciam com `r_drawflat_mode 0` sob Vulkan (funcionavam no GL). Uma sessão anterior tinha marcado isso como "não é bug, era cvar errado" — errado, reaberto e investigado com side-by-side real (mesmos cvars nos dois renderers). Causa real: `vk_world.c` escrevia a flag "esta superfície é drawflat" em `push.causticsEnabled` (offset 172 do push-constant), mas `vk_world_flat.frag` lê esse dado de um campo dele mesmo chamado `drawflatColor`, que fica no offset 124 — campo que na struct C tem esse mesmo nome e nunca era escrito (ficava zerado pelo `memset`). Comentário antigo dizia que os dois offsets coincidiam; estava errado, confirmado calculando os offsets campo a campo (std430: mat4=64B, vec4=16B, float=4B). Fix: trocar para `push.drawflatColor = ...`. Confirmado ao vivo com screenshot lado a lado GL vs Vulkan.
- **World index buffer (e demais buffers `once_per_frame`/`reuse_per_frame`) agora preferem o heap `DEVICE_LOCAL | HOST_VISIBLE`** que GPUs discretas expõem (maior com Resizable BAR, presente na RX 6800 XT desta sessão), com fallback automático para `HOST_VISIBLE | HOST_COHERENT` puro se o device não tiver esse heap. Não usa staging/`vkCmdCopyBuffer` — a doc do Vulkan confirma que isso seria contraproducente para um buffer reescrito todo frame (adicionaria uma cópia GPU extra em vez de eliminar overhead); a abordagem certa pra esse padrão é esse heap combinado, que permite `memcpy` direto como hoje. Novo: `VK_BufferPreferredMemoryType()` (vk_buffers.c) e `VK_CreateBufferResourceWithSelector()` (vk_resources.c). Testado ao vivo, sem crash, sem regressão visual.
- **`VK_TimeRefresh` agora mede tempo de GPU real via `VkQueryPool`** (timestamps), além do wall-clock antigo, porque a métrica antiga incluía acquire/present e não era comparável à do GL (que nunca chama SwapBuffers nas 128 iterações). Reset do query pool tem que rodar fora de qualquer render pass ativo (confirmado na spec) — feito uma única vez via `VK_BeginImmediateCommands` antes do loop, não a cada iteração.
- Também desta sessão: colapso do double-loop de draw do mundo em `vk_world.c` (uma única partição opaco/blended, não duas varreduras com `continue`), e fix de um bug recorrente de corrupção de descriptor set (`VkDescriptorSet was destroyed or updated without UPDATE_AFTER_BIND`) — `VK_WorldFlatSkyDescriptorSet` atualizava o set a cada superfície flat/sky em vez de uma vez por frame.

**Pendente pra próxima sessão: Task #4 — migrar `vk_world.c` para bindless** (textured/lightmapped/alpha_textured/flat/overlay), usando a mesma infraestrutura já provada em `vk_aliasmodel.c`/`vk_texture.c` (`VK_TextureBindlessDescriptorSetLayout`/`VK_TextureBindlessDescriptorSet`). Ainda não iniciada — Tiago pediu pra decidir a estratégia (um pipeline por vez / tudo de uma vez / plano detalhado primeiro) na próxima sessão. Risco real de regressão: é a mesma área de código do bug de corrupção de descriptor set corrigido nesta sessão.

### Parte 6 — Teste ao vivo com Tiago: outline funcionando (bug de ruleset), hang real no vid_restart do preset eyecandy, CORRIGIDO, tudo commitado e enviado

Sessão de teste ao vivo real com Tiago olhando a tela (não automação sozinha). Resultado:

- **Cáusticas, drawflat (todas variações), vsync adaptativo, screenshot**: confirmados funcionando.
- **Outline de mundo**: inicialmente "continua sem funcionar" mesmo com `gl_outline 3`. Causa raiz real, achada e corrigida: `R_DrawWorldOutlines()` (`src/r_brushmodel_surfaces.c`, função COMPARTILHADA pelos 3 backends, bug pré-existente, não introduzido nesta sessão) chamava `RuleSets_DisallowModelOutline(NULL)` — o gate de outline de *modelo*, que para `mod==NULL` exige cheats/demo playback — em vez de `RuleSets_AllowEdgeOutline()`, o gate correto de outline de *mundo/edge* (só restringe ruleset `rs_qcon`, sem exigir cheats). O GLM's `GL_FramebufferStartWorldNormals` já usava a função certa; só essa função compartilhada estava com a função errada. Corrigido — outline de mundo agora funciona com `gl_outline 3` sozinho, sem precisar de `devmap`/cheats, igual ao GL.
- **Mipmap (`gl_texturemode GL_NEAREST` vs default)**: Tiago não conseguiu perceber diferença mesmo testando corretamente (de longe, corredor/distância). Investigação de código não achou bug — lógica de `hasMipmap`/`maxLod` parece correta. Aceito como "sem diferença perceptual relevante que valha mais investigação agora" — pode ser efeito real mas sutil demais pra notar sem comparação A/B por screenshot.
- **FPS baixo notado por Tiago**: era o parâmetro `-dev` (usado em todos os testes de sessão anterior) ativando as validation layers do Vulkan, que têm overhead conhecido — comportamento intencional/pré-existente, não uma regressão. Confirmado: sem `-dev`, FPS normal.
- **Bug real e grave achado por Tiago**: trocar de preset gráfico pra "high eyecandy" (`exec cfg/gfx_gl_higheyecandy.cfg`, que dispara `vid_restart` no final) **travava o processo** (hang, não crash — `Get-Process` mostrava `Responding: False` e nunca mais respondia; log parava logo após "Ping tree has been created", sem nenhum erro). Não acontecia antes do trabalho desta sessão. Reproduzido de forma confiável.
  - **Causa raiz encontrada com certeza** (Opus, leitura de código): os 4 recursos novos de composição do outline em `src/vk_draw.c` (`worldOutlinePipeline`, `worldOutlinePipelineLayout`, `worldOutlineDescriptorSetLayout`, `worldNormalsSampler`) nunca eram destruídos em lugar nenhum — nem em `VK_HudResourcesShutdown()` (que destrói os equivalentes do post-process, `postProcessPipeline`/`postProcessSampler`/etc, mas parava antes de chegar nos do outline). Isso causava dois problemas simultâneos: (a) vazamento de handle a cada `vid_restart` (mesmo pipeline/layout/sampler nunca liberado antes do `VkDevice` morrer), e (b) mais grave — como `worldOutlinePipeline` nunca voltava a `VK_NULL_HANDLE`, o early-out em `VK_WorldOutlineCreatePipeline()` (`if (worldOutlinePipeline != VK_NULL_HANDLE) return true;`) fazia o primeiro frame pós-restart reusar pipeline/layout/sampler/set do **device antigo já destruído**, travando o driver na submissão do command buffer — sem gerar nenhum erro de validação (não estávamos com `-dev` no teste), só hang silencioso. Bate exatamente com o sintoma: log mostra `vid_restart` completando com sucesso, hang no frame seguinte.
  - **Fix aplicado**: adicionado o par de destruição faltante em `VK_HudResourcesShutdown()`, mesmo padrão guarda-`VK_NULL_HANDLE` já usado pros recursos do post-process, logo depois deles.
  - **Testado ao vivo por Tiago depois do fix**: trocou o preset manualmente, funcionou sem travar. Confirmado resolvido.
- **Nota de processo**: durante os testes automatizados de retry (antes do Tiago testar manualmente), sobrou uma tecla "presa" via `keybd_event` que fez o jogo parecer estar "mudando de preset sozinho" por um instante — resolvido encerrando o processo de teste; não era um bug novo, resíduo da própria automação.

**Commitado e enviado** (autorizado explicitamente por Tiago): 2 commits em `feature/sdl3-vulkan-pr` — `9452d70a` (SDL2→SDL3 finalização) e `c05de798` (cáusticas + outline + fixes de cvar + screenshot), mais um terceiro commit com o fix do hang do vid_restart. Push feito para `origin` (atualiza a PR #1145 existente) e sincronizado com o repositório separado `tibazera/ezquakevulkan`.

### Parte 5 — Bug real de sincronização achado ao tentar validar visualmente: screenshot Vulkan sempre gerava erro de validação, CORRIGIDO

Ao tentar validar visualmente os 4 fixes da Parte 4 (Tiago liberou o teclado, "SendKeys"/`AppActivate` confirmados funcionando de verdade quando ninguém mais mexe no teclado ao mesmo tempo — ver nota de método abaixo), todo `screenshot` no Vulkan disparava um erro de validação real: `vkQueueSubmit(): pSubmits[0] performs a layout transition on presentable VkImage ..., but the image has not been acquired from VkSwapchainKHR ... (either never or since the last present operation)`.

**Causa raiz**: `VK_Screenshot` (`vk_main.c`) lia `vk_options.swapChain.images[vk_options.frame.imageIndex]` diretamente e assumia que essa imagem estava em `PRESENT_SRC_KHR`. Mas `frame.imageIndex` só é atualizado dentro de `VK_BeginFrame` (via `vkAcquireNextImageKHR`) e o comando `screenshot` roda fora do ciclo de frame (é um comando de console, disparado do loop de eventos) — segundo o spec Vulkan, uma imagem de swapchain só pertence à aplicação entre ser devolvida por `vkAcquireNextImageKHR` e ser liberada de volta por `vkQueuePresentKHR`; fora dessa janela (que é exatamente onde o comando `screenshot` roda), tocar na imagem é hazard real, não só um warning cosmético.

**Fix aplicado** (`vk_main.c`, `vk_resources.c`, `vk_local.h`): `VK_Screenshot` agora faz seu próprio ciclo dedicado de `vkAcquireNextImageKHR` → copia o conteúdo (é uma leitura do que já estava sendo exibido, então captura exatamente o que um screenshot deveria capturar, sem desenhar nada novo) → `vkQueuePresentKHR` de volta sem modificação (invisível ao usuário, só devolve a imagem pro swapchain como o spec exige). Nova função `VK_EndImmediateCommandsAfter(cmd, waitSemaphore, waitStage)` em `vk_resources.c` (variante de `VK_EndImmediateCommands` que aceita um semáforo de espera no submit, necessário pra garantir que a cópia só rode depois que o acquire sinalizar de verdade).

**Confirmado corrigido**: testado ao vivo, `screenshot` gerando `Wrote ezquakeXXX.jpg` sem nenhum erro de `image has not been acquired` no log (antes do fix, toda captura gerava esse erro).

**Limitação de automação encontrada, ainda não resolvida**: em várias tentativas seguidas (algumas com restart completo do processo, que resolve o problema de foco "preso" que aparece depois de várias trocas de janela via `AppActivate`/`SendKeys`/`ESC` — confirmado que reiniciar do zero é a forma confiável de recuperar isso), a tecla **W** enviada via `keybd_event` pra andar sempre acabou sendo capturada como **texto de chat** (`TIBA: W` aparece no log/HUD) em vez de mover o personagem — mesmo em tentativas onde o mapa foi carregado 100% via linha de comando (`+gl_caustics 1 +map aerowalk`, sem nenhum `~`/comando de console antes), o console aparecia aberto no screenshot seguinte. Não foi possível determinar a causa exata sem ver a tela ao vivo (hipótese não confirmada: o jogo pode abrir o console automaticamente após certos eventos de carregamento de mapa, ou o "W" físico via `keybd_event` — diferente do `SendKeys` de texto — está sendo roteado de forma diferente por alguma razão de foco/hook de teclado). **Resultado**: não consegui validar visualmente (por screenshot real da cena, sem o console cobrindo a tela) se a cáustica aparece corretamente na água, nem o efeito do `gl_texturemode GL_NEAREST`. Ambos os 4 fixes da Parte 4 continuam confirmados via log/estabilidade (compilam, carregam mapa, não crasham, e o de `vid_vsync -1` tem confirmação funcional direta no log), mas a confirmação visual fica pendente — melhor Tiago validar direto olhando a tela, ou uma sessão futura tentar de novo com outra estratégia de automação de input (talvez um `.cfg` de bind que force `wait`-free movement, ou investigar por que o console está reabrindo sozinho).

## Sessão 2026-08-05 (Claude, Windows — port SDL2→SDL3 finalizado + gaps Vulkan de paridade vs. GLC/GLM)

**Contexto**: sessão longa cobrindo 3 frentes em sequência: (1) fechar de vez a migração SDL2→SDL3 (itens que ficaram pendentes de sessões anteriores), (2) criar um repositório novo `tibazera/ezquakevulkan` (privado, GitHub) como espaço isolado pra acompanhar as diferenças do nosso fork Vulkan+SDL3 contra o upstream `QW-Group/ezquake-source`, e (3) trabalho autônomo overnight (autorizado explicitamente por Tiago antes de dormir) implementando e testando ao vivo os 2 gaps de paridade funcional achados na comparação Vulkan vs. GLC/GLM: cáusticas subaquáticas (`gl_caustics`) e outline de mundo (`gl_outline` bit 2).

### Parte 4 — Auditoria de cvars multi-valor (0/1/2/3...) no Vulkan + segunda passada SDL3, 4 gaps reais corrigidos

Pedido explícito do Tiago (após reparar que meus testes anteriores do Gap 1 nunca tinham ativado `gl_caustics 1` de verdade, só rodado com o default desligado): investigar se o port GL→Vulkan e a migração SDL2→SDL3 deixaram passar despercebidos valores específicos de cvars com múltiplos inteiros/enum (não só liga/desliga). Consultado o Opus de novo, achou **4 gaps reais confirmados** (a cvar É lida no Vulkan, mas nem todo valor produz o efeito certo) e 0 gaps na segunda passada de enums SDL3 (área já bem coberta por sessões anteriores). Todos os 4 corrigidos nesta sessão:

1. **`vid_framebuffer_fxaa` (0-17) — CORRIGIDO.** GL mapeia pra 17 presets reais do header FXAA 3.11 da NVIDIA (`GL_FramebufferFxaaPreset`, `gl_framebuffer.c:1009-1017`); o Vulkan colapsava tudo em bool (`push.fxaaEnabled = ... != 0`). Como o post-process Vulkan usa uma implementação própria simplificada (não o header FXAA real — ver comentário já existente em `vk_post_process.frag` explicando por quê), replicar 17 variantes de shader não é viável. Solução: novo campo `fxaaQuality` (float 0-1, derivado de `preset/17.0f`) controla continuamente os dois parâmetros reais do algoritmo simplificado — limiar de detecção de borda (`0.1→0.05`) e força do blend (`0.50→1.00`). Não é bit-a-bit idêntico ao FXAA real por preset, mas agora o valor da cvar produz diferença real e monotônica, em vez de nada. Arquivos: `vk_draw.c` (struct `vk_post_process_push_t` + `VK_PostProcessComposite`), `vulkan_shaders/vk_post_process.frag`.
2. **`gl_texturemode` (6 modos GL) — CORRIGIDO.** Os 2 modos sem mipmap (`GL_NEAREST`/`GL_LINEAR`) não desligavam mipmapping de verdade no Vulkan — `VK_FilterFromMinification` mapeava `nearest` e `nearest_mipmap_nearest` pro mesmo par `(filter, mipmapMode)`, e o sampler sempre usava `maxLod = VK_LOD_CLAMP_NONE` (mipmap completo). Corrigido: `VK_FilterFromMinification` agora também retorna `hasMipmap` (false só para os 2 modos sem sufixo `_mipmap_`), e o cache de sampler (`VK_TextureCachedSampler`/`VK_SamplerCacheIndex`) ganhou essa dimensão extra — quando `!hasMipmap`, `maxLod = 0.25f` (trick padrão pra travar no mip 0). Cache dobrou de tamanho (`VK_SAMPLER_CACHE_SIZE` agora tem um fator `* 2 /* hasMipmap */` a mais). Arquivo: `vk_texture.c`.
3. **`vid_vsync -1` (adaptive) — CORRIGIDO E CONFIRMADO NO LOG.** GL trata `-1` como um terceiro caso (`SDL_GL_SetSwapInterval(-1)`); o Vulkan tratava qualquer valor não-zero (incluindo `-1`) como `r_swapInterval.integer` truthy → sempre `FIFO_KHR` puro, nunca alcançando `FIFO_RELAXED_KHR` (o equivalente Vulkan real de vsync adaptativo, que já estava na lista de presentation modes preferidos mas nunca era alcançado). Corrigido com uma segunda lista de preferência (`preferredModesAdaptive`) só com `FIFO_RELAXED`→`FIFO` como fallback, escolhida quando `r_swapInterval.integer < 0`. **Testado ao vivo e confirmado no log**: `vulkan: selected present mode 3` (FIFO_RELAXED) com `vid_vsync -1` — antes desse fix teria sido mode 2 (FIFO puro). Arquivo: `vk_physical_devices.c`.
4. **`vid_gammacorrection` (0/1/2) — CORRIGIDO.** GL distingue "tentar sRGB, aceitar fallback" (1) de "exigir sRGB, rejeitar o device se não tiver" (2) — ver a escada `vid_options[]` em `vid_sdl.c`. O Vulkan tratava 1 e 2 de forma idêntica (mesmo `req_color_space`, mesmo fallback silencioso pra qualquer colorspace disponível). Corrigido: quando `vid_gammacorrection.integer == 2` e o fallback de formato não encontrar um colorspace sRGB exato, `VK_PhysicalDeviceSwapChainCompatible` agora retorna `false` — o que já faz `VK_SelectPhysicalDevice` rejeitar aquele device específico (`continue` pro próximo, comportamento pré-existente, não modificado) em vez de aceitar silenciosamente um colorspace errado. Testado ao vivo com `vid_gammacorrection 2`: o device AMD RX 6800 XT tem suporte sRGB normal, então não foi rejeitado — não foi possível confirmar visualmente o caminho de rejeição sem um device sem suporte sRGB à mão, mas a lógica foi lida com cuidado e o `continue` do call site já era testado/funcional antes desta mudança. Arquivo: `vk_physical_devices.c`.

**Duas cvars que motivaram a investigação original (`gl_outline`, `r_drawflat`/`r_drawflat_mode`) já estavam corretas e completas** — confirmado lendo o código, o Opus não achou gap novo ali (o outline bit 2/mundo é o trabalho ainda incompleto de integração, não um gap de "valor não tratado", ver Gap 2 acima na Parte 3).

**Descoberta importante sobre o protocolo de teste desta sessão**: `SendKeys`/`AppActivate`/`PostMessage(WM_CHAR)` — testados os 3 — **nenhum consegue injetar texto no console do ezQuake** rodando localmente nesta máquina (confirmado repetidamente com um `echo MARCADOR_UNICO` de verificação que nunca apareceu no log, mesmo com a janela em foco e ninguém mais mexendo no teclado). O jogo deve ler input via SDL3 de baixo nível (raw/DirectInput-like), que ignora eventos sintéticos de janela do Win32. **Método que funciona de verdade**: passar tudo via linha de comando na hora de lançar o processo (`+cvar valor +map nome` encadeados, ou `+exec arquivo.cfg` com os comandos dentro) — confirmado funcionando repetidas vezes (título da janela mostra o mapa carregado, cvars aplicam, log mostra o efeito quando há um). **Limitação real**: sem forma de injetar comando depois que o processo já está rodando, não dá pra tirar `screenshot` depois que o mapa carregou dentro do mesmo processo (testado com `wait`/`cl_maxfps 1` dentro do `.cfg` pra dar tempo real antes do `screenshot` — não funcionou, `wait` é tick de simulação, não tempo de parede, e o screenshot sempre saiu cedo demais, ainda na tela de loading). Validação visual real (a cáustica/mipmap parecendo certos) **precisa de alguém olhando a tela ao vivo** — não é algo que consegui automatizar sozinho nesta sessão.

Todos os 4 fixes compilam limpo e foram testados ao vivo quanto a estabilidade (carrega mapa, não crasha, roda) — 1 deles (`vid_vsync -1`) tem confirmação funcional direta no log (`selected present mode 3`), os outros 3 só têm confirmação de "não quebra", não de "produz o resultado visual esperado". **Nada commitado ainda.**

### Parte 1 — Port SDL2→SDL3 finalizado

Além dos fixes já registrados em sessões anteriores (áudio init check, `refresh_rate` float, IME `SDL_SetTextInputArea`), fechado nesta sessão:

- **Migração completa dos nomes SDL2 legados** (joystick, eventos, GL context, mutex/semáforo, atomic, CPU count) para os nomes nativos SDL3, em todos os arquivos que ainda dependiam do shim `SDL_oldnames.h`. `SDL_ENABLE_OLD_NAMES` **removido** do `CMakeLists.txt` — o build compila/linka limpo sem o shim, confirmando que não sobrou nenhum resíduo.
- **Todos os ~34 includes SDL do projeto (26 arquivos) prefixados com `SDL3/`** (`#include <SDL3/SDL.h>` etc), e o hack `find_path(... PATH_SUFFIXES SDL3)` removido do CMake — o target `SDL3::SDL3-static` do vcpkg já resolve o include path sozinho.
- **Rename de arquivos**: `vid_sdl2.c`→`vid_sdl.c`, `in_sdl2.c`→`in_sdl.c`, `sys_sdl2.c`→`sys_sdl.c` (via `git mv`, preservando detecção de rename), `CMakeLists.txt` e os 2 comentários que citavam o nome antigo atualizados.
- **`build-linux.sh` corrigido**: ainda listava pacotes SDL2 reais (`libsdl2-dev`, `SDL2-devel`, `sdl2`) em 4 distros — trocado pelos equivalentes SDL3.
- **Cvar novo `joy_id`** (`SDL_JoystickID` estável, default `-1` = desativado) adicionado em paralelo ao `joyindex` existente (índice posicional, semântica SDL2, mantida intocada) — decisão explícita do Tiago de não quebrar configs salvos. Quando `joy_id >= 0`, tem prioridade e resolve por ID real (`IN_OpenJoystickId`), resiliente a hot-plug. Documentado em `help_variables.json`.
- Textos "SDL2" corrigidos para "SDL3" em `README.md` e `help_commands.json` (4 descrições de comando visíveis ao jogador).
- `SDL_syswm.h` morto removido de `vid_sdl.c` (nunca existiu no SDL3, confirmado que o `#if SDL_MAJOR_VERSION < 3` nunca disparava e nada dependia dele).
- **Fix de bug real encontrado numa revisão do Opus**: `menu_options.c:533` gravava `refresh_rate` (float) direto no cvar `r_displayRefresh` (int) sem arredondar — mesmo bug já corrigido em `vid_sdl.c` em sessão anterior, só que esse site tinha ficado de fora. Corrigido com o mesmo padrão `(float)(int)(x + 0.5f)`.
- **Fix de robustez**: `text_input_area_set` (flag estática em `IN_UpdateTextInputState`) não sobrevivia a `vid_restart` — removida, a função agora chama `SDL_SetTextInputArea` sempre que necessário em vez de cachear estado.

**Não commitado ainda** — tudo em diff local no worktree.

### Parte 2 — Repositório novo `tibazera/ezquakevulkan`

Criado no GitHub (privado, independente, sem histórico do upstream — snapshot do estado atual deste worktree como primeiro commit). **Cuidado ao reproduzir**: durante a criação, um `git init` acidental quase reafirmou o link de worktree do repo Android por engano (`.git` como arquivo-ponteiro tinha sido copiado junto pelo robocopy do worktree de origem) — identificado e corrigido antes de qualquer push; nenhum dano real aos worktrees existentes. Lição: ao clonar/copiar um worktree linked (não o repo principal) pra virar a base de um repo novo, sempre checar e remover o arquivo `.git` (ponteiro) copiado junto antes de rodar `git init` no destino.

### Parte 3 — Auditoria comparativa Vulkan vs. GLC/GLM (Opus) — 2 gaps reais encontrados

Não é regressão do port SDL3 (código de input/menu/keys confirmado byte-idêntico ao upstream) — são gaps de **escopo** do backend Vulkan, que ainda não implementava 2 coisas que GLC/GLM têm:

1. **Cáusticas subaquáticas (`gl_caustics`)** — `vk_world.c` descartava o parâmetro (`(void)caustics;`).
2. **Outline de mundo (`gl_outline` bit 2)** — nunca implementado no Vulkan (só outline de *modelo* existe, `vk_aliasmodel.c`). Já era gap conhecido de sessões anteriores, confirmado ainda válido.

Consultado o Opus de novo pra projetar solução implementável dos dois (não só identificar) — relatório completo com plano passo a passo pra cada um, resumido abaixo junto do que foi de fato implementado.

### Gap 1 — Cáusticas: IMPLEMENTADO E TESTADO AO VIVO, funcionando

Seguido o plano do Opus quase à risca:

- **`src/vk_world.c`**: campo `padding` (último) de `vk_world_push_t` reaproveitado como `causticsEnabled` (struct continua 176 bytes, sem crescer — só esse slot era pura folga de alinhamento, e `vk_world_flat`'s próprio bloco GLSL local já tratava esse offset como `drawflatColor`, então o C-side que escrevia nele foi renomeado mas continua escrevendo no mesmo byte-offset). Novo par `VK_WorldCausticsTextureReady()`/`VK_WorldCausticsDescriptorSet()` espelhando o padrão já existente de `VK_WorldDetailTextureReady`/`VK_WorldDetailDescriptorSet`, com fallback pra `solidwhite_texture` (nunca `VK_NULL_HANDLE`, layout do pipeline fica fixo). `vk_world_draw_t` ganhou campo `caustics`. **Decisão de design seguindo GLM** (não GLC): a granularidade de "quem recebe cáustica" é 100% decidida no fragment shader pelo bit `EZQ_SURFACE_UNDERWATER` já existente no `inFlags` por-vértice (gravado desde sempre em `vk_main.c:148`, só nunca consumido) — o parâmetro `caustics` por-modelo de `VK_DrawBrushModel` continua ali só pra bater a assinatura do `renderer_api_t` compartilhado, mas é ignorado na composição (documentado com comentário explicando a escolha).
- **Descriptor sets**: adicionado mais um set (cáustica) nos pipelines `worldTextured` (2→3), `worldLightmapped` (3→4, no limite garantido pelo spec Vulkan de `maxBoundDescriptorSets`, sem checagem de device runtime — igual ao padrão já existente pros outros pipelines, nenhum já checava isso) e `worldAlphaTextured` (2→3). `worldAlphaTextured` **não tinha o atributo `inFlags` no vertex input** (só position/texcoord/detail_coords) — adicionado (`VkVertexInputAttributeDescription[4]`, novo location 3 lendo `vbo_world_vert_t.flags`), e o `.vert`/`.frag` correspondentes atualizados pra repassar/ler o flag. Isso importa porque é justamente o pipeline blended (água/lava com `r_wateralpha`) onde a cáustica é mais visível no Quake original.
- **Shaders** (`vk_world_textured.frag`, `vk_world_lightmapped.frag`, `vk_world_alpha_textured.{vert,frag}`): novo `sampler2D causticsTexture[2]` no set seguinte ao de detail; lógica de UV animada + blend multiplicativo idêntica à referência GLM (`draw_world.fragment.glsl`, fator `-0.1234375` = `-3*(0.5/64)`), aplicada **depois** do detail texture (mesma ordem do GLM), gateada por `causticsEnabled > 0.5 && (inFlags & EZQ_SURFACE_UNDERWATER) != 0u`.
- **Asset**: `underwatertexture` (`textures/water_caustic`) já era carregado em código 100% compartilhado (`r_rmisc.c:70`, `R_InitOtherTextures`) — nada novo a carregar, só passou a ser referenciado pelo lado Vulkan.

**Testado ao vivo** (protocolo pedido pelo Tiago: `map <nome>`, esperar ~20s, segurar W ~4-5s pra andar, checar log): ciclo completo em `dm3` → `aerowalk` → `schloss` → `ztndm3` → `dm3` de novo, com `-dev -condebug +set vid_renderer 2`. Build compila limpo em todas as iterações, processo estável e responsivo em todos os mapas, sem crash/TDR. `dm6` não estava disponível no basedir local de teste (só `dm3` do id1, mais `aerowalk`/`schloss`/`ztndm3` de PK3 extra) — pendente testar se algum dia esse mapa for adicionado ao ambiente.

**Achado importante durante o teste, não é bug da cáustica**: apareceu um erro sério em cascata no log (`VkDescriptorSet ... was destroyed or updated without UPDATE_AFTER_BIND`, seguido de `commandBuffer must be in the recording state` repetido ~220-240x por sessão). **Confirmado via teste A/B com `git stash`** (rebuild sem as mudanças de cáustica, mesmo teste em `aerowalk`) que esse hazard é **pré-existente**, idêntico em contagem com e sem a mudança — é o mesmo hazard de sincronização de descriptor-set já documentado extensivamente na seção "Sessão 2026-07-23 (Claude, Windows, parte 2 — bug de modelos pretos/TDR)" mais abaixo (causa raiz #3/#4, upload de textura e free de descriptor set síncronos em frame ativo). **Não é causado nem agravado pela cáustica** — meu código de cáustica não introduziu nenhuma ocorrência nova. Continua sendo a mesma investigação em aberto de sessões anteriores (mipmap NPOT / descriptor set lifetime), não escopo deste trabalho.

**Ainda não commitado** — diff local, pronto pra revisão. Recomendação: revisar visualmente (Tiago vendo a tela de verdade) antes de commitar — o teste automatizado confirma "não quebrou nada e não crasha", mas não confirma que a cáustica está visualmente correta (cor, animação, intensidade) sem alguém olhando a água de um mapa que a tenha.

### Gap 2 — Outline de mundo (`gl_outline` bit 2): PARCIALMENTE IMPLEMENTADO, BLOQUEADO num ponto de integração de fluxo — não testado ao vivo, requer trabalho adicional antes de tentar rodar

**Decisões de arquitetura tomadas e já implementadas** (seguindo o plano do Opus, com pesquisa web adicional confirmando a técnica de normal antes de codar — ver nota sobre `dFdx/dFdy` abaixo):

- **Rejeitada** a réplica literal do MRT do GLM (segundo color attachment no render pass principal) — mexeria na matriz MSAA×post-process já existente (4 combinações). Risco desproporcional.
- **Rejeitada** a alternativa "edge-detect só no depth buffer existente" — a imagem de depth atual não tem `SAMPLED_BIT`, é N-sample com MSAA, perde cantos entre superfícies coplanares.
- **Implementado**: render pass **separado e independente**, `vk_renderpass_worldnormals` (`src/vk_renderpass.c`), single-sample por construção, 2 attachments (color RGBA16F via `VK_WorldNormalsFormat()`, depth próprio single-sample — não reaproveita `vk_options.swapChain.depthImage`, que é N-sample quando MSAA está ativo). `LOAD_OP_CLEAR` obrigatório no color (composição distingue "desenhado aqui" de "nada aqui" pelo canal alfa).
- **Implementado**: recursos de imagem/framebuffer/descriptor pool em `src/vk_swapchain.c` (`VK_CreateWorldNormalsResources`/`VK_DestroyWorldNormalsResources`), um conjunto por imagem de swapchain (mesmo padrão do post-process), alocados **incondicionalmente** junto do resto (`VK_CreateSwapChainFramebuffers`/`VK_DestroySwapChainFramebuffers`) porque `gl_outline` é unlatched. **Testado ao vivo que a alocação em si não quebra nada** (`map dm3`, sem os erros novos de criação de recurso — só o hazard pré-existente de sempre, ver Gap 1).
- **Implementado**: 3 shaders novos —
  - `src/vulkan_shaders/vk_world_normals.vert`/`.frag`: redesenha a geometria de mundo (só `inPosition`, sem textura/lightmap) e escreve `vec4(normal, depth)` no color attachment. A normal é reconstruída no fragment shader via `normalize(cross(dFdx(worldPos), dFdy(worldPos)))` — **pesquisado ativamente na web antes de implementar** (não só seguindo o plano do Opus às cegas): confirmado que essa é a técnica padrão e correta pra flat shading de geometria totalmente plana (que é o caso do BSP do Quake) — dá a normal EXATA do triângulo, não uma aproximação, ao contrário das técnicas de reconstrução de normal a partir de depth buffer (que são pra quando não se tem acesso à geometria original, não é o nosso caso). Fontes: artigo sobre reconstrução de normal via depth (usado só como comparação/descarte) e discussões de fórum Khronos confirmando `cross(dFdx(worldPos), dFdy(worldPos))` como o idiom padrão. `depth` usa `distance(worldPos, cameraPos)/zFar` em vez do `abs(viewZ/zFar)` do GLM (o push constant do Vulkan já é a matriz MVP combinada, sem MV separado disponível no shader pra recuperar Z de view space) — ambos são medidas de profundidade monotônicas ao longo do raio de visão, o que é tudo que o teste de diferença finita do shader de outline precisa; não é bit-a-bit idêntico ao GLM mas deve produzir resultado visual equivalente.
  - `src/vulkan_shaders/vk_world_outline.frag`: porte direto do algoritmo de `src/glsl/fx_world_geometry.fragment.glsl` (mesmo teste de descontinuidade de normal + segunda derivada de depth), lendo o color attachment do pass acima.
- **Implementado**: pipeline de composição (`VK_WorldOutlineCreatePipeline`/`VK_WorldOutlineComposite` em `src/vk_draw.c`), fullscreen triangle (reaproveita o `.vert` do post-process, que já é procedural sem input), blend `r_blendfunc_premultiplied_alpha` (equivalente a alpha-over reto já que o shader só emite alpha 0 ou 1), sampler dedicado `VK_FILTER_NEAREST` (o shader original usa `texelFetch`, então filtragem bilinear borraria exatamente as bordas que o algoritmo tenta medir). Cvars lidas: `gl_outline_color_world`, `gl_outline_world_depth_threshold` (bound 1-16), `gl_outline_world_normal_threshold` (bound 0-0.999), escala por `VID_ScaledWidth3D()/VID_ScaledHeight3D()` igual ao `GLM_DrawWorldOutlines`.
- 3 novos entry points registrados em `src/vk_local.h`: `VK_WorldNormalsRenderPass()`, `VK_CreateWorldNormalsResources()`/`VK_DestroyWorldNormalsResources()`/`VK_WorldNormalsFramebuffer()`, `VK_WorldOutlineActive()`/`VK_WorldOutlineComposite()`/`VK_WorldNormalsTransitionForSampling()` (este último é um no-op documentado — o render pass já deixa o attachment em `SHADER_READ_ONLY_OPTIMAL` como `finalLayout`, sem barreira manual necessária, ao contrário do post-process). 3 shaders novos registrados no `CMakeLists.txt` via `add_vulkan_shader`. **Tudo isso compila e linka limpo, e os 3 shaders compilam pra SPIR-V sem erro** (confirmado, inclusive o uso de `dFdx`/`dFdy` em GLSL 450 core, que o glslang aceitou de primeira).

**Decisão de compatibilidade confirmada com Tiago (importante pra quando for testar/documentar)**: no GLM, `gl_outline 2/3` só funciona de verdade com 4 comandos: `vid_renderer 1`, `vid_framebuffer 1`, `r_drawflat 1`, `gl_picmip 33` (achado real no código: `GL_FramebufferStartWorldNormals`, `gl_framebuffer.c:487/493`, retorna `false` sem um framebuffer FBO alocado, que só existe com `vid_framebuffer 1|2` — `r_drawflat`/`gl_picmip 33` não são requisito técnico do outline em si, são um truque visual à parte do Tiago pra deixar as texturas lisas e os contornos mais visíveis). **Confirmado e decidido**: `gl_outline` já É uma cvar de verdade compartilhada entre os 3 backends (bit 1, outline de modelo, já roda hoje no Vulkan via gate em `cl_ents.c:215`, código comum, não específico de renderer — a doc antiga "Requires vid_renderer 1" em `help_variables.json` estava desatualizada/incompleta). **Não criar `vk_outline` separado.** `vid_framebuffer` é uma cvar específica de GLC/GLM que nunca existiu no Vulkan (o post-process Vulkan é outro sistema, sempre alocado, sem cvar de ativação equivalente) — a arquitetura de render pass separado que foi implementada aqui **não depende dela e não deve criar essa dependência artificial**: quando a integração final estiver pronta, `gl_outline 3` sozinho deve bastar no Vulkan (sem precisar de `vid_framebuffer`/`r_drawflat`/`gl_picmip`). `help_variables.json`'s `gl_outline` já foi atualizado nesta sessão pra documentar isso: "On Vulkan (vid_renderer 2) works standalone, no vid_framebuffer needed."

**Teste ao vivo desta manhã, achado importante sobre o protocolo de teste**: rodei de novo com `gl_caustics 1` ativado explicitamente (sessão anterior só tinha testado com as cvars DESLIGADAS — `gl_caustics`/`gl_outline` são `"0"` por default, então o teste anterior só provava "não quebra nada", nunca exercitou o shader novo de verdade). Confirmado: `textures/water_caustic.png` existe dentro de `ezquake.pk3` (`unzip -l` confirma o path exato batendo com `R_LoadTextureImage("textures/water_caustic", ...)` em `r_rmisc.c:70`), então o asset não é o problema. **Ainda não validei visualmente se a cáustica aparece de verdade** — a tentativa de usar `noclip`+`screenshot` via `SendKeys` não confirmou que os comandos chegaram ao console do jogo (o log mostra só mensagens de location do MVDSV, não uma confirmação de screenshot salvo nem de `noclip` ligado) — o foco de janela via `SendKeys`/`AppActivate` não é 100% confiável neste ambiente. **Pendência real pra próxima verificação**: confirmar que comandos batem no console (ex: usar `echo` de teste antes de comandos reais, ou validar por outro sinal no log) e então validar visualmente (via screenshot de verdade, salvo em `qw/`) que a cáustica aparece nadando na água de `aerowalk` com `gl_caustics 1`.

**BLOQUEIO RESOLVIDO (sessão atual)** — o problema de ORDEM identificado antes era real: `worldDraws[]` só fica populado dentro de `R_DrawWorld()` → `VK_DrawWorld()` → `VK_WorldQueueModel()`, chamado de `R_RenderView()` (`src/r_rmain.c:889`), que roda **depois** de `VK_BeginFrame()` já ter feito `vkCmdBeginRenderPass` do main render pass. Ou seja: no único ponto em que a lista está pronta, o command buffer já está gravando dentro do main pass, e render passes não são aninháveis.

**Solução adotada: rota 1 (intercalar render passes no mesmo command buffer)** — implementada e compilando limpo. Em `VK_RenderView()` (`src/vk_world.c`), logo depois de os buffers de vértice/índice estarem prontos e **antes** de qualquer draw de mundo: se `VK_WorldOutlineActive()` → `vkCmdEndRenderPass` → `VK_DrawWorldNormalsPass()` (abre o pass de normais, redesenha `worldDraws[]` só com position/mvp, pula os `blended`, fecha) → `VK_WorldBeginMainRenderPassNoClear()` (reabre o main pass com a variante `vk_renderpass_main_noclear`, replicando a mesma escolha de framebuffer do `VK_BeginFrame`: offscreen do post-process quando ativo, swapchain caso contrário). O `VK_WorldOutlineComposite()` é chamado depois do batch opaco, no mesmo slot que o `GLM_RenderView` usa (logo após `GLM_DrawWorldModelBatch(opaque_world)`, antes de alias models/sprites/alpha).

**Por que fechar o main pass exatamente aí é lossless**: a variante noclear preserva a cor, mas o depth attachment limpa nas DUAS variantes (`VK_RenderPassCreateVariant`, espelhando `GL_Clear()`). Como nesse ponto o primeiro trecho do main pass não tinha nada além do próprio clear, re-limpar o depth não perde nada. Fazer isso mais tarde (ex: depois do loop opaco) jogaria fora o depth do mundo.

**Validação contra o FTEQW** (pesquisa feita nesta sessão, código real lido de `engine/vk/`): o FTEQW faz exatamente esse mesmo padrão, e ele é idiomático no motor deles, não uma gambiarra. Pontos concretos:
- `engine/vk/vk_backend.c`, `T_Gen_CurrentRender()`: para materializar `$currentrender` no meio do frame ele faz `vkCmdEndRenderPass` → trabalho fora do pass → `vkCmdBeginRenderPass(..., &vk.rendertarg->restartinfo, ...)`, reabrindo o pass no MESMO command buffer.
- `engine/vk/vk_init.c` `VK_GetRenderPass()` + `engine/vk/vkrenderer.h:414`: eles mantêm variantes de render pass indexadas por política de load — `RP_RESUME` (`LOAD_OP_LOAD` em cor e depth), `RP_FULLCLEAR`, `RP_DEPTHCLEAR`, `RP_DEPTHONLY` (shadowmaps) — e `VKBE_RT_Begin()` troca o pass pra `RP_RESUME` depois do primeiro begin justamente pra "future reuse shouldn't clear stuff". É o análogo direto do nosso `vk_renderpass_main_noclear`.
- Diferença arquitetural que vale registrar: o FTEQW **separa** as fases ("build batches" em `BE_GenModelBatches()`, depois `VKBE_SubmitMeshes()` grava), então em tese conseguiria montar o pass auxiliar antes de abrir o principal — mas mesmo tendo essa opção, na prática usa end/begin no meio do frame. Isso confirma que a rota 1 não é um workaround imposto pela nossa arquitetura fundida (ezQuake monta a lista dentro do mesmo loop que grava): é a escolha que o motor mais próximo do nosso também faz. A rota 2 (separar fases no ezQuake, mexendo em código compartilhado entre os 3 backends) fica descartada — muito mais invasiva pro mesmo resultado.

**Diferença deliberada em relação ao GLM**: o GLM escreve as normais via MRT (segundo color attachment nos próprios shaders de mundo, `GL_FramebufferStartWorldNormals`). Replicar isso no Vulkan exigiria dar um segundo color attachment às cinco pipelines de mundo mais uma variante de main render pass pra cada combinação de MSAA/post-process. Redesenhar a geometria position-only num pass dedicado e minúsculo não toca nenhuma pipeline existente — o custo é uma segunda passada sobre os vértices do mundo, que é justamente por isso que tudo é gateado em `VK_WorldOutlineActive()`.

**Arquivos tocados pro Gap 2**: `src/vk_renderpass.c`, `src/vk_swapchain.c`, `src/vk_local.h`, `src/vk_draw.c`, `src/vk_world.c`, `CMakeLists.txt`, `src/vulkan_shaders/vk_world_normals.vert` (novo), `src/vulkan_shaders/vk_world_normals.frag` (novo), `src/vulkan_shaders/vk_world_outline.frag` (novo). **Build limpo confirmado** (MSVC x64 RelWithDebInfo, os 3 shaders novos compilam pra SPIR-V, link OK).

**Pendência**: validação visual ao vivo (`gl_outline 3` sozinho, sem `vid_framebuffer`) — não feita, é escopo de outra etapa. Verificar também o comportamento com MSAA ligado (o pass de normais é sempre single-sample por construção) e com post-process ativo (o reabrir do main pass escolhe o framebuffer offscreen nesse caso, caminho ainda não exercitado em runtime).

**Nada disso foi commitado** — diff local. Como está inacabado e não testado visualmente, considerar isolar esse trabalho (ex: branch separada ou stash) do resto do PR se o Tiago quiser commitar só o Gap 1 (cáusticas, que está completo e testado) antes de terminar o Gap 2.


## Sessão 2026-07-23 (Claude, Windows, parte 2 — bug de modelos pretos/TDR) — EM ANDAMENTO, TDR grave aconteceu, ler antes de continuar

**Gatilho**: Ciscon mandou um tar.xz com sua pasta Quake real (`https://nicotinelounge.com/quake/backups/quake.tar.xz`) pra reproduzir o bug de "modelos pretos/transparentes + itens sem textura" que ele reportou no Vulkan (ver seção anterior, item 4 "Itens pretos"). Baixado e extraído (com autorização do Tiago) em `E:\Projetos Linux\_quake-test-data\quake` (fora deste repositório — dados de jogo, não versionar). Faltavam alguns PK3 (`base.pk3` real, só o `.bak` veio) por causa de symlinks quebrados no tar, mas `id1/pak0.pak`+`pak1.pak` e as demos de duelo dele (`qw/duel/*.qwd`) vieram OK.

**Como reproduzir**: copiar `E:\Projetos Linux\ezquake-sdl3-vulkan-pr\build-msvc-x64\RelWithDebInfo\ezquake.exe` pra DENTRO de `E:\Projetos Linux\_quake-test-data\quake\ezquake.exe` (não usar `-basedir`/`-nohome`, não funcionou — o exe precisa estar fisicamente na pasta, porque `com_basedir` é derivado do path do próprio executável) e rodar de lá: `E:\Projetos Linux\_quake-test-data\quake\ezquake.exe -dev -condebug +set vid_renderer 2`, depois `map dm3` no console. **Confirmado reproduzido pelo Tiago no Windows** (mesma família de driver AMD/RADV que a máquina do Ciscon): armas/modelos pretos e transparentes, itens no chão coloridos sem textura nenhuma — bate exatamente com o relato original. Log fica em `E:\Projetos Linux\_quake-test-data\quake\qw\qconsole.log` (cumulativo entre execuções — sempre `Remove-Item` antes de um teste novo se quiser isolar só a sessão atual).

### Causa raiz #1, CONFIRMADA E CORRIGIDA: pipeline overlay sem o atributo `inFlags`

`VK_WorldCreateOverlayPipeline` (`src/vk_world.c`, usada por `worldLumaPipeline`/`worldFullbrightPipeline`) reutiliza o shader `vk_world_textured_vert_spv`, que ganhou um atributo de vértice `inFlags` (location 3) numa sessão anterior (fix de `r_drawflat_mode` tinted/bright, ver seção de sessão Linux acima). Essa função nunca foi atualizada pra declarar esse atributo — só tinha `attributeDescriptions[3]` (índices 0-2), faltando o 3. Confirmado por erro real de validation layer: `vkCreateGraphicsPipelines(): pCreateInfos[0].pVertexInputState->pVertexAttributeDescriptions does not have a Location 3, but [VK_SHADER_STAGE_VERTEX_BIT] has [Input variable, Location 3, "inFlags"]`. **Corrigido**: array agora `[4]`, com o `attributeDescriptions[3]` (location 3, `VK_FORMAT_R32_UINT`, offset de `flags`) copiado do padrão já usado em `VK_WorldCreateTexturedPipeline`. Confirmado que esse erro específico sumiu do log depois do fix — mas **sozinho não resolveu o bug visual**, só era uma causa concorrente.

### Causa raiz #2, já estava no build (de sessão anterior, não desta): frames-in-flight vs. semáforos

`VK_MAX_FRAMES_IN_FLIGHT` 2→3 em `src/vk_local.h` (pra casar com as 3 imagens de swapchain desde o fix de vsync) + `renderFinishedSemaphores` reindexado por swapchain imageIndex em vez de frame-in-flight, em `src/vk_main.c`. Corrigia um erro de validação separado (`vkQueueSubmit(): pSubmits[0].pSignalSemaphores[0] ... may still be in use by VkSwapchainKHR`), confirmado que sumiu do log — mas também **não resolveu sozinho** o bug visual.

### Causa raiz #3, CONFIRMADA E CORRIGIDA (mas incompleta — ver TDR abaixo): upload de textura síncrono em frame ativo

Achado real: `VK_UploadTexture` (`src/vk_texture.c`) sempre destruía/recriava o descriptor set de uma textura de forma síncrona, mesmo quando chamada NO MEIO da gravação de um command buffer já ativo (`vk_options.frame.active == true`) — por exemplo, ao pegar um item que carrega um ícone de HUD ou skin pela primeira vez. `vkDeviceWaitIdle` (já existente em `VK_TextureDestroyObjects`) protege contra a GPU (trabalho já submetido), mas NÃO desfaz um `vkCmdBindDescriptorSets` já gravado na CPU no command buffer do frame atual, ainda não submetido — daí o erro `VkDescriptorSet ... was destroyed or updated without UPDATE_AFTER_BIND` seguido de cascata de "commandBuffer must be in the recording state", corrompendo o resto do frame.

**Fix aplicado** (`src/vk_texture.c`, `src/vk_main.c`, `src/vk_local.h`): `VK_UploadTexture` original renomeada pra `VK_UploadTextureImmediate` (static). Novo `VK_UploadTexture` público: se `!vk_options.frame.active` → imediato como antes (map load, init, vid_restart — comportamento inalterado); se `frame.active` → enfileira numa fila nova `deferredTextureUploads[]` (cópia própria dos pixels, já que o caller libera o buffer original logo depois). Nova `VK_TextureApplyDeferredUploads()` chamada em `VK_BeginFrame` (`vk_main.c` ~linha 630), ANTES de `vkResetCommandBuffer`/`vkBeginCommandBuffer` — ponto limpo onde nenhum command buffer referencia o descriptor set antigo ainda.

**Confirmado que ISSO SOZINHO NÃO RESOLVEU** — Tiago testou de novo depois desse fix (+ os dois anteriores juntos) e reportou: os modelos/itens já aparecem pretos/sem textura **desde o carregamento inicial do mapa**, ANTES de pegar qualquer item. O log confirma: o erro `UPDATE_AFTER_BIND` aparece logo após "The Abandoned Base"/"ciscon entered the game" (nome do mapa carregado, primeiro ou segundo frame), não em resposta a nenhum evento de gameplay. Ou seja, "pegar item" era só uma coincidência de timing no teste anterior — o hazard real acontece já no primeiro frame pós-load, por um caminho ainda não identificado (suspeitas não confirmadas: outro setter de textura síncrono tipo `VK_TextureWrapModeClamp`/anisotropia rodando durante o load de textura de mapa; ou o load de mapa entrando em `frame.active` de alguma forma inesperada; ver prompt completo passado pro agente de investigação, não repetido aqui).

**Investigação de continuação disparada** (agente Opus, rodando em paralelo, resultado ainda não recebido no momento em que este texto foi escrito) — pedido pra: confirmar se `frame.active` é realmente false durante todo o carregamento de mapa; mapear TODOS os call sites que destroem/atualizam um `vk_texture_t.descriptorSet` (não só os 3 já cobertos); achar a causa raiz real do crash no primeiro frame pós-load; implementar fix; compilar (sem rodar/testar visualmente).

### INCIDENTE GRAVE: TDR/travamento total do Windows

Durante os testes acima (não confirmado em qual etapa exata — pode ter sido ao testar o build com os 3 fixes juntos, rodando `map dm3` repetidamente com validation layers ligadas), **o Windows inteiro travou por ~3 minutos e voltou com um popup de erro "-4 vulkan"** — sintoma de TDR (Timeout Detection and Recovery) do driver AMD, o mesmo tipo de incidente já documentado no `AGENTS.md` como acontecido antes ("vid_restart TDR fix (Vulkan/AMD)", causa raiz na época: command buffer mid-frame + pipeline vazado). **NÃO investigado ainda nesta sessão** — prioridade após recuperar a máquina é: (1) confirmar que a máquina está estável, (2) verificar `qw/qconsole.log` e o Visor de Eventos do Windows por qualquer indício de qual operação especificamente travou o driver, (3) considerar se algum dos 3 fixes desta sessão (especialmente o de frames-in-flight/semáforos, que mexe em sincronização de baixo nível) introduziu um novo risco de TDR, ou se é o próprio bug de descriptor-set-corrompido-em-uso (causa raiz #3) que, sem fix completo, pode estar deixando a GPU num estado inválido grave o suficiente pra travar o driver inteiro, não só corromper o frame visualmente.

**Estado do worktree no momento deste incidente** (preservar, não descartar): `git status --short` mostra modificados `src/vid_sdl2.c` (fix alt-tab, já resumido na seção anterior), `src/vk_local.h`, `src/vk_main.c`, `src/vk_texture.c`, `src/vk_world.c` (os 3 fixes desta seção) — nada commitado ainda, tudo é diff local. Também não-rastreados: `AGENTS.md`, `PR_DESCRIPTION.md`, `_build_wip.bat`, `build_after_rebase.log`, `racat.cfg` (config de teste do racat, não essencial, pode ignorar).

**Regra a seguir ao retomar**: NÃO assumir que os 3 fixes de código estão corretos só porque compilam — o TDR é evidência de que pelo menos um cenário de teste levou a GPU a um estado ruim o suficiente pra precisar reset do driver. Antes de continuar testando ao vivo, esperar o resultado da investigação do agente (causa raiz #4, o crash no primeiro frame), aplicar esse fix também, e só então testar de novo — com cautela (considerar testar sem validation layers primeiro, ou com um timeout curto, pra não travar a máquina de novo caso o problema persista). Não fazer commit/push de nenhum desses fixes até confirmar visualmente que o bug de modelos pretos está resolvido E que não há mais indício de TDR.

**Erro exato confirmado pelo Tiago**: `vkQueueSubmit failed: -4` — código -4 é `VK_ERROR_DEVICE_LOST`. Isso é a causa direta reportada pelo próprio driver antes do TDR/travamento do Windows: a GPU/driver considerou o device perdido (geralmente por um comando inválido/corrompido submetido de verdade à fila, não só um erro de validação que teria sido pego ANTES do submit). Reforça a hipótese de que o command buffer corrompido pela causa raiz #3 (ou a #4 ainda não identificada) não está só gerando avisos de validação — em pelo menos uma execução, algo realmente inválido chegou a ser submetido pra GPU de verdade e travou o driver. Ao retomar: procurar `VK_ERROR_DEVICE_LOST`/`vkQueueSubmit failed` no código (`vk_main.c`) pra ver como isso é tratado hoje (provavelmente só loga e talvez tente continuar, o que seria perigoso se o device já estiver morto) — considerar se precisa de um `Sys_Error` explícito nesse caso em vez de tentar seguir renderizando com um device inválido.

### Causa raiz #4, CONFIRMADA E CORRIGIDA (terceiro caminho de descriptor set, achado só depois de grep exaustivo no repo inteiro)

Depois dos fixes #3 (upload de textura) e do fix dos setters de filtering/anisotropia/clamp (chamado de "causa raiz #3 continuação" na investigação), o bug **ainda persistia** — mesmo padrão, logo após "You got the shells", mesmo descriptor set, 420 ocorrências no log, processo eventualmente crashava (sem travar o Windows dessa vez, mas ainda arriscado).

Grep exaustivo (`vkUpdateDescriptorSets`/`vkFreeDescriptorSets`/`vkAllocateDescriptorSets`/`vkDestroyDescriptorPool`/`vkResetDescriptorPool`) em TODO o `src/` achou o terceiro caminho, nunca coberto pelos fixes anteriores: `R_TextureAllocateSlot()` (`r_texture.c:450`) → `R_DeleteTexture()` → `VK_TextureDelete` → `VK_TextureDestroyObjects` → `vkFreeDescriptorSets`, **síncrono, sem nenhum gate em `frame.active`**. Isso dispara sempre que o jogo recarrega uma textura sobre um slot já existente com tamanho diferente — exatamente o padrão de recarregar ícone de HUD/munição ou skin de jogador/arma ao pegar um item. Roda a partir de código de HUD/gameplay durante `SCR_UpdateScreen`, no meio do frame, depois que os draws de mundo/HUD daquele frame já tinham gravado `vkCmdBindDescriptorSets` contra o set antigo — free ali é exatamente o hazard. Não é coberto pelo deferral do `VK_UploadTexture` porque a exclusão do slot acontece antes/independente de qualquer upload.

**Fix aplicado** (`src/vk_texture.c`, terceira fila seguindo o mesmo padrão): `deferredDescriptorFrees[]`. Em `VK_TextureDestroyObjects`, quando `frame.active`, o handle do descriptor set vai pra fila em vez de ser liberado na hora (o `memset` ainda zera `vktex->descriptorSet`, então o slot pode ser reusado no mesmo frame com um set novo e distinto). Fora de frame ativo (map load/vid_restart/shutdown) ou fila cheia, continua liberando na hora como antes. `VK_TextureApplyDeferredUploads` (mesmo ponto de sempre, início do `VK_BeginFrame`, antes de `vkBeginCommandBuffer`) agora libera essa fila PRIMEIRO, antes dos uploads (recupera capacidade do pool antes de qualquer alocação nova). Fila zerada em `VK_TextureDiscardDeferredUploads`/`VK_TextureInitialiseState` (que já destroem o pool inteiro).

Grep exaustivo também confirmou que os outros 2 `vkUpdateDescriptorSets` do projeto (`vk_draw.c:655`, post-process; `vk_world.c:363`, sky) NÃO são esse hazard — post-process escreve uma vez só por imagem de swapchain (cache lazy, nunca re-escreve um set já usado); sky atualiza um set per-frame-in-flight e faz bind no MESMO frame (update-then-bind, não o padrão "já gravado num frame anterior ainda executando").

**Build limpo, compilado, MAS AINDA NÃO TESTADO AO VIVO no momento em que este texto foi escrito** — o agente que implementou foi instruído a não rodar o jogo (risco de TDR já materializado uma vez nesta sessão). Confiança alta do agente de que esse é o terceiro/último caminho, mas isso precisa ser confirmado testando de verdade antes de considerar o bug resolvido. Se persistir mesmo depois deste fix, o próximo lugar a olhar (sugestão do próprio agente) é se `frame.active` pode ficar `false` numa janela estreita entre `vkBeginCommandBuffer` (`vk_main.c:639`) e `frame.active=true` (`vk_main.c:683`) — ele não acha que é isso, mas não descartou 100%.

**Testado SEM `-dev` (sem validation layers) — bug visual PERSISTE.** Confirmado pelo Tiago: mesmo sem validação (nenhum erro no `qconsole.log`), modelos/itens continuam pretos/sem textura, e apareceu um crackling de áudio novo (provável sintoma de stress/contenção, não necessariamente causa separada). **Conclusão importante**: os 3 fixes de deferral de descriptor set eram reais e corrigiam um hazard genuíno de sincronização (validado por validation layer), mas **não são a causa raiz do bug visual em si** — o "preto"/"sem textura" acontece por outro motivo, independente desse hazard. Não descartar os 3 fixes (continuam corretos e necessários), mas a investigação precisa mudar de eixo: de "sincronização/lifetime de descriptor set" para "conteúdo/pipeline de renderização real desses draws" (sampler, layout de imagem, formato, ou dado de textura em si chegando errado/vazio na GPU).

### Causa raiz #5, EM INVESTIGAÇÃO: conteúdo de mipmap corrompido para texturas NPOT (skins de modelo/itens)

Investigação de pipeline (vertex attributes de `vk_aliasmodel.c`/`vk_sprite3d.c`, descriptor set layout, blend state) comparada contra GLC/GLM não achou nenhum descompasso estrutural — tudo bate. Duas hipóteses restantes: (1) timing/readiness de textura (upload adiado por engano), (2) conteúdo real da textura errado (mipmap malformado).

**Instrumentação de diagnóstico ativada temporariamente**: `VK_AliasDebugLog` (`src/vk_aliasmodel.c` ~linha 93, era no-op) agora imprime de verdade via `Con_Printf`, com uma chamada nova em `VK_AliasQueuePreparedDraw` (~linha 552) logando `texIdx`/`ready`/`weapon`/`player`/`mode` por draw. **Resultado do teste ao vivo (sem `-dev`, só pra isolar o log)**: ~512973 linhas, **100% `ready=1`**, nunca `skippedTexture`. Isso **descarta definitivamente a hipótese #1** (timing) — a textura está sempre marcada pronta quando o draw é enfileirado, tanto pra arma (`texIdx=488 weapon=1`) quanto pra itens (`texIdx=566`/`576`, mode=0). **Confirma a hipótese #2**: textura pronta e bindada certo, mas o CONTEÚDO na GPU está errado.

Suspeita concreta: `VK_BuildMipPyramid` (`src/vk_texture.c` ~linha 715-740) gera a pirâmide via `Image_MipReduce` (`src/image.c` ~linha 376) chamado **in-place** (mesmo buffer como `in` e `out`: `Image_MipReduce(outBuffer + offset, outBuffer + offset, &width, &height, 4)`, linha ~731). Skins de modelo/itens usam `TEX_NOSCALE` com frequência (`src/r_aliasmodel_skins.c` ~linhas 94-96, 158-160), ou seja, dimensões NPOT reais — bem diferente de texturas de mundo do Quake original, que tipicamente são POT (64x64, 128x128), por isso nunca expuseram esse caminho antes. Não confirmado ainda se o in-place quebra matematicamente pra alguma combinação de dimensão NPOT/ímpar — próxima investigação (agente disparado, resultado ainda não recebido) foi instruída a auditar `Image_MipReduce` byte a byte, incluindo os casos de dimensão 1 numa das duas dimensões (função tem mais código depois do trecho já lido), e conferir se os offsets/dimensões de cada `VkBufferImageCopy` batem exatamente com o que foi gerado na CPU.

**Lembrete pra quando isso for resolvido**: a instrumentação de debug (`VK_AliasDebugLog` + a chamada de log em `VK_AliasQueuePreparedDraw`) precisa ser revertida/removida antes de considerar o trabalho pronto — regra geral do projeto, não deixar log de debug temporário na versão final (ver AGENTS.md).

**Efeito colateral do log de diagnóstico, corrigido**: com o log ativo (`Con_Printf` chamado a cada draw de alias model, ~512973 linhas numa sessão curta, `qconsole.log` chegou a 1.68 milhões de linhas), o volume de I/O de console competia por CPU com o thread de áudio e causou crackling audível — não é bug do Windows nem dos fixes de Vulkan, é efeito direto do próprio log de diagnóstico. `VK_AliasDebugLog` já foi revertido pra no-op (`src/vk_aliasmodel.c` ~linha 93-99), mas a chamada em `VK_AliasQueuePreparedDraw` (~linha 559) continua no código, pronta pra reativar rápido se precisar — inofensiva com a função em modo no-op.

**Investigação de continuação (agente Opus) FALHOU por limite de sessão da API** (não é erro de código, resetou 23:30 horário de São Paulo) — não chegou a uma conclusão nem aplicou fix algum. Progresso que ele tinha antes de cair:

- Confirmou (lendo `R_TextureSizeRoundUp`) que quando `r_texture_support_non_power_of_two` está true (Vulkan seta isso), essa função retorna dimensões NPOT sem arredondar — skins de alias model chegam em `VK_UploadTexture` com largura/altura genuinamente NPOT no Vulkan. Confirma que esse é o caminho não-testado (texturas de mundo do Quake original são tipicamente POT).
- Tentou rastrear `Image_MipReduce` (`src/image.c` linha 376+) em vários casos de borda NPOT (largura 3, altura/largura=1) tentando achar corrupção matemática no reduce in-place — não achou o bug concreto, a lógica parecia correta nesses casos específicos.
- Próximo passo mais concreto, ainda não feito: comparar `VK_TextureMipLevelCount` (contagem de níveis esperada) contra a contagem real gerada por `VK_BuildMipPyramid` para uma dimensão NPOT pequena tipo 32x1 — se divergirem, é a causa (nível esperado pela imagem Vulkan nunca populado com dado real, lendo lixo/preto).

**Nada foi commitado nesta sessão inteira**. Todos os 6 fixes/mudanças de código (alt-tab grab, pipeline overlay inFlags, frames-in-flight/semáforo, upload de textura deferred, setters de textura deferred, descriptor-set-free deferred) continuam como diff local no worktree, intactos.

**Sessão pausada aqui por decisão do Tiago** (máquina já sofreu 1 TDR grave nesta sessão; investigação de mipmap não convergiu depois de 3 rodadas de agente + 1 análise direta). Última tentativa de análise direta (sem agente, sessão principal) descartou mais alguns candidatos sem achar a causa:

- `VK_TextureUploadBufferToImageImmediate` (a função real que faz a cópia buffer→imagem pra ambos os casos com/sem mipmap) parece correta: transição de layout, `vkCmdCopyBufferToImage` com as regions calculadas por nível, transição de volta — nada óbvio errado.
- `VK_TextureRecordTransitionBarrier` cobre `levelCount = max(1, vktex->mipLevels)` corretamente — barreira não está limitada só ao nível 0.
- Conversão de paleta→RGBA das skins acontece antes de `R_LoadTexturePixels`/`R_LoadTexture`, que já espera RGBA de 4 bytes — não vi indício de alpha=0/RGB=0 sendo produzido ali (mas não segui esse caminho até a origem real da conversão de paleta, só confirmei que o formato de entrada em `R_LoadTexturePixels` já é RGBA).
- Tentativa de achar uma função tipo `VK_TextureMipLevelCount` mencionada pelo agente anterior — **não existe no código com esse nome**, pode ter sido um nome hipotético/memória falsa do agente, ou uma função que ele pretendia escrever, não uma já existente. Não confundir isso com um call site real ao retomar.

**Candidatos ainda não descartados, pra retomar**: (1) a lógica exata de `Image_MipReduce` pra combinações NPOT muito específicas (o agente tentou e não achou, mas não teve tempo de cobrir todos os casos antes de cair por limite de sessão); (2) se `R_TextureSizeRoundUp`/o caminho de picmip faz alguma suposição de POT em outro lugar que quebra silenciosamente pra dimensões NPOT reais quando picmip/max_size está ativo (não investigado ainda); (3) algo relacionado à origem da conversão paleta→RGBA das skins especificamente (não world textures), ainda não seguido até a fonte.

**Recomendação concreta pra retomar**: antes de mais leitura de código, considerar despejar/inspecionar visualmente os bytes reais de uma skin conhecida (dimensões, alguns pixels de cada nível de mip) via log temporário — é mais rápido confirmar/descartar por dado real do que continuar deduzindo estaticamente. Cuidado: NÃO usar `Con_Printf` em alto volume de novo (causou crackling de áudio nesta sessão, ver nota acima) — se for logar, limitar a um print único por textura carregada, não por draw/frame.

## Regra permanente (Tiago pediu explicitamente, sessão 2026-07-22)

Manter este arquivo (`CONTINUE.md`, maiúsculo — é o mesmo slot de arquivo que `continue.md` em filesystems case-insensitive como Windows, não criar um `continue.md` separado) atualizado sempre que uma sessão avançar ou pausar, tanto aqui quanto em `E:\Projetos Linux\ezquake-source\continue.md` (worktree Android, esse sim minúsculo, filesystem diferente/caso não colide lá). Objetivo: qualquer sessão futura (Claude ou Codex, Windows ou Linux) sabe onde o trabalho parou.

## Sessão 2026-07-23 (Claude, Linux, `/home/tiba/src/ezquake-source`) — 4 bugs reportados pelo Ciscon testando o build da sessão anterior

**Contexto**: Ciscon (outro tester) testou o build Linux gerado na sessão de 2026-07-22 (mesma máquina do Tiago, mesma família de GPU AMD/RADV) e reportou 4 problemas: itens pretos, `r_drawflat_mode` 1/2 sem efeito, outlines não funcionando, `vid_vsync` não respeitando modo imediato. Investigado com o Fable 5 (duas consultas dedicadas, sempre pedindo pra ele ler o código real antes de opinar — ver regra na seção de 2026-07-22 Windows) + verificação ao vivo nesta máquina (AMD RX 6800 XT, Mesa RADV, Wayland, monitor 360Hz).

**Ainda sem commit/push no momento em que este texto foi escrito** — aguardando autorização explícita do Tiago pra commitar as mudanças de código abaixo (só o próprio CONTINUE.md pode já ter sido commitado, conferir `git log` antes de assumir).

### 1. Outline do mundo (`gl_outline 2`/`3`) — NÃO é bug, é gap conhecido

Ciscon tinha testado especificamente outline de *mundo* (confirmado com o Tiago). Bit 2 (`gl_outline & 2`, outline de geometria do mundo via MRT + edge-detect) simplesmente não existe na árvore atual — nenhum `worldNormals*`/`vk_world_normals.*` em lugar nenhum. **Achado importante do Fable**: o CONTINUE.md antigo (seção Windows) descreve esse trabalho como "implementado então desabilitado", mas isso nunca foi commitado neste branch — ficou só numa sessão local/Windows não sincronizada. Tratar CONTINUE.md como log narrativo, não como fonte de verdade do que está na árvore — sempre `grep`/ler o HEAD real antes de assumir que algo existe.

Outline de *modelo* (bit 1, `VK_ALIAS_MODE_OUTLINE` em `vk_aliasmodel.c`) foi auditado pelo Fable via leitura estática (dispatch, gating por ruleset, pipeline sempre criado, ordem de draw) e parece correto — não mexido nesta sessão.

**Decisão explícita do Tiago nesta sessão**: outline de mundo (bit 2) fica pra uma sessão futura de propósito — não é um bug a corrigir, é uma feature inteira a implementar (segundo attachment de cor pra normais + passo de pós-processamento de edge-detect, ver notas de arquitetura na seção "Sessão 2026-07-22 (Claude, Windows)" mais abaixo, incluindo os cuidados com MSAA). Perguntado e adiado deliberadamente — não reabrir como "ainda não corrigido" numa próxima sessão sem checar aqui primeiro.

### 2. `r_drawflat_mode` 1 (tinted) / 2 (bright) não tinha efeito nenhum no Vulkan — CORRIGIDO E CONFIRMADO

Causa raiz (achada pelo Fable, confirmada lendo o código): `r_refdef2.drawFlatFloors`/`drawFlatWalls` (`src/cl_view.c:1024-1025`, compartilhado pelas 3 renderers) só fica `true` quando `r_drawflat_mode == 0` — isso só controla se a superfície vai pro chain "flat puro" (`vk_world_flat.frag`, sem textura) ou pro chain de textura normal. GLC/GLM não dependem desse gate pra tinted/bright: eles reaplicam a cor por cima da textura real dentro do PRÓPRIO shader texturizado (`applyColorTinting()` em `draw_world.fragment.glsl`, gateado só por `r_drawflat.integer`, não pelo mode). Os shaders texturizados do Vulkan (`vk_world_textured.frag`, `vk_world_lightmapped.frag`) não tinham nenhum equivalente — por isso mode 1/2 renderizava 100% textura normal, sem efeito algum.

**Não mexer em `cl_view.c`** (avisado pelo Tiago em tempo real) — isso afetaria GLC/GLM também. O fix ficou inteiramente no lado Vulkan:

- `src/vk_world.c`: `vk_world_push_t` ganhou `floorColor`/`wallColor`/`drawflatMode`/`tintFloors`/`tintWalls` (struct de 144 → 176 bytes — acima do mínimo garantido de 128 do Vulkan mas dentro dos 256 típicos de desktop AMD/NVIDIA/Intel; **não portar pro branch Android** sem reconferir o limite lá). Preenchidos no loop de draw só quando `r_drawflat_mode != 0` (mode 0 continua 100% do pipeline `vk_world_flat` de sempre, intocado).
- Novo atributo de vértice `flags` (location 3/4 conforme o pipeline) adicionado em `VK_WorldCreateTexturedPipeline`/`VK_WorldCreateLightmappedPipeline` — já existia no VBO compartilhado (`vbo_world_vert_t.flags`, com o bit `EZQ_SURFACE_IS_FLOOR` já preenchido em `vk_main.c:147` desde sempre) mas nenhum pipeline texturizado consumia. **Não precisou mudar o VBO/vertex builder** — só passou a ler o que já existia.
- `src/vulkan_shaders/vk_world_textured.{vert,frag}` e `vk_world_lightmapped.{vert,frag}`: recebem o novo atributo, portam `applyDrawflatTint()` (equivalente ao `applyColorTinting()` do GLM: tinted = multiply, bright = recolor por luminância).
- **Testado e confirmado visualmente pelo Tiago** (`r_floorcolor 255 0 0` / `r_wallcolor 0 255 0` com `r_drawflat_mode 1` → chão vermelho / parede verde, textura ainda visível por baixo).
- Achado à parte durante a investigação, não corrigido (fora de escopo, documentado pelo Fable): `R_SetNonPowerOfTwoSupport()` só é chamado do init GL (`vid_common_gl.c`), nunca do Vulkan — `r_texture_support_non_power_of_two` fica sempre `false` no Vulkan, forçando resample de toda textura NPOT. Provavelmente invisível na maioria dos casos (a maioria das skins MDL já é POT) mas é um bug real, renderer-wide, pendente.

### 3. `vid_vsync 0` não usava modo imediato — CORRIGIDO E CONFIRMADO (com pegadinha)

Duas causas, achadas em duas rodadas:

**3a. Ordem de preferência errada** (`src/vk_physical_devices.c`, `VK_PhysicalDeviceBestPresentationMode`): com vsync off, a lista de preferência tentava `MAILBOX_KHR` antes de `IMMEDIATE_KHR`. MAILBOX ainda é sincronizado com a tela (troca de frame em vez de bloquear, mas sem tearing) — diferente do `SDL_GL_SetSwapInterval(0)` real que GLC/GLM usam. Trocada a ordem pra `IMMEDIATE` primeiro quando `r_swapInterval.integer == 0`.

**3b. Só a ordem não bastou** — Ciscon (e depois o próprio Tiago) confirmaram log mostrando `IMMEDIATE` corretamente selecionado, mas o FPS continuava travado no refresh do monitor (360). Causa: `VK_CreateSwapChain` (`src/vk_swapchain.c:402-411`) só pedia um buffer extra (`minImageCount + 1`) para `MAILBOX`, nunca para `IMMEDIATE` — rodando com só 2 imagens. Nesse Wayland/RADV específico, o *release* dos buffers de volta pro app parece ficar pautado pelo próprio ritmo de repaint do compositor a menos que `wp_tearing_control_v1` seja negociado entre driver e compositor (não universal) — com só 2 imagens isso vira um cap efetivo de fps no refresh rate, apesar do present mode certo. Estendida a condição do `+1` pra cobrir `IMMEDIATE` também.

**Confirmado com `timedemo` (`qw/matchinfo/demos/weirdrocket.qwd`, 25581 frames) rodando local, sem servidor/rede no caminho**:
- `vid_vsync 0` + `cl_maxfps 0`: **1727.2 fps** (bem acima do monitor de 360Hz — antes ficava preso em ~360 mesmo com maxfps liberado).
- `vid_vsync 1`: jogo interativo normal (`map dm3`) funciona bem, sem travar.

**Bug novo encontrado, não corrigido, baixa prioridade**: `timedemo` combinado com `vid_vsync 1` (FIFO) trava o processo (CPU cai a ~0%, estado sleeping, nunca termina/imprime relatório). Interativo com `vid_vsync 1` funciona normal — parece específico da combinação timedemo+FIFO, não do gameplay normal. Provável relacionado ao `vkWaitForFences(..., UINT64_MAX)` em `VK_BeginFrame` (`src/vk_main.c`, perto de onde já existe um comentário sobre trocar timeout infinito por finito no `vkAcquireNextImageKHR` por uma razão parecida — mesma área de código, não investigado a fundo ainda). **Não iniciado.**

Diagnóstico temporário deixado no código (`Com_Printf`/`Con_Printf` com prefixo "TEMP diagnostic" em `vk_physical_devices.c` e `vk_swapchain.c`, listando present modes disponíveis e `imageCount` real) — decidir se remove antes de commitar ou deixa (é só log, não afeta comportamento).

### 4. Itens pretos — NÃO REPRODUZIDO nesta máquina, causa raiz não encontrada

Mesma família de GPU/driver (AMD/RADV) nas duas máquinas (Tiago e Ciscon), então não é claramente uma questão de fabricante — pode ser geração de GPU, versão de driver/Mesa, ou uma race condition que só se manifesta em certas condições de timing. O Fable investigou fundo (pipeline de alias models, descriptor sets, upload de textura, mip pyramid, sampler) e eliminou várias hipóteses com evidência, mas não achou a causa raiz por leitura estática — ver relatório completo dele nesta sessão (não resumido aqui por já estar bem detalhado, procurar no transcript se precisar). Sugestão dele: habilitar o log `VK_AliasDebugLog` já existente em `vk_aliasmodel.c` e testar na máquina do Ciscon, ou usar RenderDoc/validation layers lá. **Precisa da máquina do Ciscon pra progredir** — não dá pra reproduzir/depurar daqui.

### Notas técnicas gerais desta sessão

- **`sudo` não funciona de dentro do harness do Claude Code** (sem TTY pra senha) — nem via Bash nem via `!comando` do usuário. Instalação de pacotes precisou ser feita pelo próprio Tiago num terminal separado.
- **Push pro GitHub precisa de token** — sem credencial configurada na máquina por padrão. Token fine-grained do GitHub precisa explicitamente de "Contents: Read and write" nas Permissions (não só "Repository access"), senão dá 403 mesmo autenticando certo (leitura/`git ls-remote` funciona, push não).
- **Cuidado com comandos em cadeia no Bash tool desta sessão**: se um comando no meio de um script multi-linha retorna código de saída != 0 (mesmo um `pkill` sem processo pra matar, que é normal/esperado), os comandos SEGUINTES na mesma chamada não executam. Rodar `pkill`/checks-que-podem-falhar em chamadas separadas dos comandos que realmente importam (`cp`, `chmod`, etc.), não em sequência na mesma call.
- **AppImage é o método padrão agora pra empacotar builds de teste pra compartilhar** (pedido explícito do Tiago) — usar `misc/appimage/appimage-manual_creation.sh` com `EXECUTABLE`/`SKIP_DEPS=1` já setados pro binário já compilado, não o tarball manual com libs soltas usado uma vez no início desta sessão (descartado).
- **`timedemo` é a forma limpa de medir fps sem depender de olho humano/screenshot** — mas só funciona de forma confiável com `qw/autoexec.cfg` renomeado temporariamente pra fora do caminho primeiro (senão o auto-connect do config corrida com o carregamento da demo e derruba o teste no meio). Lembrar de restaurar o nome depois.

## Sessão 2026-07-22 (Claude, Linux, `/home/tiba/src/ezquake-source`, Zorin OS 18.1 / Ubuntu 24.04 "noble")

**Pedido do Tiago**: compilar o branch `feature/sdl3-vulkan-pr` (HEAD `36057234`, o mesmo commit documentado na sessão Windows acima) numa máquina Linux nova (`/home/tiba`, diferente do `/home/tiago` da sessão Codex de 2026-07-05 citada abaixo) e colocar o binário em `/home/tiba/nquake` pra ele testar. **Testado visualmente e confirmado pelo Tiago** ("Sim, abriu normalmente").

Achados relevantes pra quem for reproduzir este build em outra máquina Ubuntu/Debian-based sem `libsdl3-dev` empacotado:

1. **Ubuntu 24.04 não tem `libsdl3-dev` nos repos** (só existe no Debian testing/sid, que é por isso que o job Linux do CI roda dentro de um container `debian:testing`, não direto no runner `ubuntu-latest` — ver `.github/workflows/main.yml`). Sem SDL3 do sistema, `USE_SYSTEM_LIBS=ON` (padrão) falha o `pkg_check_modules(sdl3)`.
2. **Solução usada**: compilar SDL3 3.2.20 a partir do código-fonte (`github.com/libsdl-org/SDL`, tag `release-3.2.20`) e instalar num prefixo local não-privilegiado (`/home/tiba/src/sdl3-install`, sem `sudo`), com Wayland+X11+Vulkan+ALSA+PulseAudio habilitados (todos detectados automaticamente pelo CMake do SDL desde que os `-dev` de X11/Wayland/libdecor/etc. estejam instalados — ver lista de pacotes abaixo). `PKG_CONFIG_PATH=/home/tiba/src/sdl3-install/lib/pkgconfig` faz o `pkg-config --modversion sdl3` do CMake do ezquake achar essa instalação. O binário final carrega `libSDL3.so.0` via `RUNPATH` absoluto que o CMake já embute sozinho (confirmado com `readelf -d`), então não precisa de `LD_LIBRARY_PATH` na hora de rodar.
3. **`libvulkan-dev` do Ubuntu 24.04 (1.3.275) é headers demais antigos** — o código usa `VK_AMD_anti_lag` (`VkAntiLagDataAMD`, `VK_STRUCTURE_TYPE_ANTI_LAG_DATA_AMD` etc. em `src/vk_main.c`), extensão só presente em headers Vulkan mais recentes (~1.3.28x+). Erro de compilação: "unknown type name 'VkAntiLagDataAMD'" / "request for member ... in something not a structure or union". **Solução**: clonar `github.com/KhronosGroup/Vulkan-Headers` (branch default, header version 357 no momento) e passar `-DVulkan_INCLUDE_DIR=/home/tiba/src/Vulkan-Headers/include` no configure — a lib/loader do sistema (`libvulkan.so.1`, ABI estável) continua sendo usada normalmente, só os headers de compilação são mais novos. Não precisou trocar `libvulkan1`/driver do sistema.
4. **Comando de configure completo que funcionou**:
   ```
   export PKG_CONFIG_PATH=/home/tiba/src/sdl3-install/lib/pkgconfig
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DRENDERER_VULKAN=ON \
     -DCMAKE_PREFIX_PATH=/home/tiba/src/sdl3-install \
     -DVulkan_INCLUDE_DIR=/home/tiba/src/Vulkan-Headers/include
   cmake --build build --parallel $(nproc)
   ```
5. **Pacotes apt necessários** (além dos já listados em `build-linux.sh`/CI, que ainda faltam alguns pro SDL3 compilar do zero): `cmake ninja-build pkg-config glslang-tools libcurl4-openssl-dev libexpat1-dev libfreetype-dev libjansson-dev libjpeg-dev libminizip-dev libpcre2-dev libpng-dev libsndfile1-dev libspeex-dev libspeexdsp-dev libvulkan-dev libwayland-dev wayland-protocols libxkbcommon-dev libegl1-mesa-dev libgles2-mesa-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev libxtst-dev libpulse-dev libasound2-dev libdrm-dev libgbm-dev libdecor-0-dev libibus-1.0-dev`.
6. Binário final copiado pra `/home/tiba/nquake/ezquake-linux-x86_64` (o binário anterior que estava lá, linkado contra um SDL3 inexistente no sistema — provavelmente de uma tentativa anterior de build/CI download — foi preservado como `ezquake-linux-x86_64.bak-nosdl3`). Testar com `-vulkan` na linha de comando.
7. **Nenhuma mudança de código nesta sessão** — só descoberta de receita de build local. Nada commitado além deste próprio arquivo.
8. Sudo não funciona de dentro do harness do Claude Code neste ambiente (sem TTY pra senha, `sudo -n` falha) — os `apt-get install` precisaram ser rodados pelo próprio Tiago num terminal separado. Por isso a decisão de instalar SDL3/usar Vulkan-Headers em prefixos locais sem privilégio, evitando depender de mais `sudo` pro resto do processo.

## Sessão 2026-07-22 (Claude, Windows, worktree `E:\Projetos Linux\ezquake-sdl3-vulkan-pr`)

**5 commits novos, feitos em cima do trabalho da sessão anterior (Codex/Linux, `e5d4f778`/`48ca9805`/`c87251f5` abaixo), testados visualmente e aprovados pelo Tiago ("funcionou ok sem bugs"). Rebase feito sobre `origin/feature/sdl3-vulkan-pr` sem perda de trabalho de nenhum dos dois lados — só um conflito real em `src/vk_world.c` no branch flat (o commit remoto `48ca9805` mudou esse branch para 2 descriptor sets/lightmap; meu bind-cache foi adaptado a esse formato novo, resolvido manualmente).**

1. `973cfe9d` (era `b3ada541` antes do rebase) — cache de last-bound-pipeline/descriptor-set no loop de draw do mundo (`vk_world.c`, nova função `VK_WorldBindIfChanged`). Pula `vkCmdBindPipeline`/`vkCmdBindDescriptorSets` quando o estado já é igual ao draw anterior (comum, já que draws vêm agrupados por material). Não toca em geometria/contagem/ordem de draws. Cache é invalidado sempre que algo externo (alias models, sprites 3D, overlay luma/fullbright) faz bind no mesmo command buffer entre draws do mundo. **Achado pelo Fable 5** numa investigação dedicada de performance (ver seção logo abaixo).
2. `e3eff48b` (era `00e4ff14`) — fix do drawflat Vulkan (bug do racat): `r_drawflat_mode` só deveria ser um estilo de cor (0=normal/1=tinted/2=bright), não um gate de ativação — bug diferente do que a sessão anterior já tinha corrigido (cores erradas + falta de lightmap shading, `48ca9805`); esse aqui é sobre o `r_drawflat_mode != 0` desligando o efeito inteiro. Tinted/bright continuam lacuna conhecida no Vulkan (só "normal" implementado), documentado, não implementado — nem FTEQW nem vkQuake implementam isso.
3. `281353b3` (era `a389be91`) — fix real de CMake: `string(REGEX REPLACE ...)` sem match retorna a string original, causava `FILEVERSION` inválido no `.rc` do Windows quando o clone não tem tags alcançáveis.
4. `8a7a350a` (era `0493ea03`) — fix de build: preset `msvc-x64` não fixava `RENDERER_VULKAN=ON`, então qualquer reconfiguração do zero silenciosamente gerava um build OpenGL-only, causando "Invalid vid_renderer value detected". Corrigido fixando a flag como `cacheVariable` do preset. **Específico do preset Windows/MSVC** — não afeta Linux (ver pedido de build Linux abaixo, que usa outro preset/toolchain).
5. `1c578ad4` (era `8c801c33`) — buffers estáticos do mundo (`bufferusage_reuse_many_frames`/`bufferusage_constant_data`) agora usam memória `DEVICE_LOCAL` com staging upload, em vez de `HOST_VISIBLE|HOST_COHERENT`.

**Push feito para `origin feature/sdl3-vulkan-pr` nesta sessão** (autorizado explicitamente pelo Tiago).

**Pedido em andamento**: compilar uma build Linux para o Tiago testar no Zorin OS (ele roda a sessão Codex anterior em `/home/tiago/...` segundo o histórico abaixo, então o projeto já tem precedente de build Linux funcional — não é a primeira vez). Ver se há um preset CMake Linux/GCC ou se é `USE_SYSTEM_LIBS=ON` (mencionado no handoff Codex abaixo) + toolchain nativo.

### Investigação de performance desta sessão (Fable 5, 3 consultas)

Tiago pediu para revisar uma proposta de arquitetura de terceiros (RHI, migração por sistema, instancing, command buffers secundários, HUD SDF) e depois focar especificamente em performance real ainda não explorada.

**Veredito sobre a proposta de RHI/arquitetura**: a maior parte já existe no projeto, só com nomes diferentes — `renderer_api_t` (`r_renderer.h`) já é a "RHI" (dispatch `renderer.DrawWorld()` etc. pros 3 backends), migração por sistema já é como os arquivos são organizados (`vk_world.c`, `vk_aliasmodel.c`, etc.), shaders já são um-por-sistema em `vulkan_shaders/`. **Não vale aplicar** — não é modernização, é redescoberta com outro vocabulário.

**Itens genuinamente ausentes, investigados e descartados por ora**:
- Instancing de alias models: infra existe mas vestigial (binding de instância nunca alimentado, `instanceCount=1` hardcoded). Exigiria mover push-constants pra buffer de instância + reescrever shader. Volume de alias models simultâneos em QuakeWorld é baixo — não vale o esforço/risco agora.
- Command buffers secundários: zero threading de render no projeto, nenhum profiling mediu gravação de comandos como gargalo. Não vale.
- HUD batched com SDF: HUD já tem batching parcial (`VK_HudDrawImages`), overhead de draw calls do HUD é trivial frente ao mundo 3D. Não vale.

**Achado real e aplicado**: bind cache de pipeline/descriptor-set no loop do mundo (commit 1 acima) — única coisa encontrada com ganho mensurável em CPU-bound desktop, risco baixo.

**Push constants do mundo**: no momento da investigação, 144 bytes (acima dos 128 garantidos pelo spec Vulkan) — nota: a sessão Codex anterior (`48ca9805`) também mexeu nesse struct (fog + drawflatColor), então esse número pode ter mudado depois do rebase; conferir `sizeof(vk_world_push_t)` de novo antes de assumir. Funciona hoje (AMD/Adreno/Mali topo de linha suportam mais), mas é risco de portabilidade, não performance.

### Trabalho pendente identificado mas não iniciado: outline de mundo no Vulkan

`gl_outline` é bitmask: bit 1 (modelos) já funciona no Vulkan (`VK_ALIAS_MODE_OUTLINE`, `vk_aliasmodel.c`). Bit 2 (mundo) não existe no Vulkan ainda — relacionado mas DIFERENTE do "world outlines" mencionado na auditoria GLM→Vulkan da sessão Codex abaixo (aquela lista trata de outros gaps de paridade visual; conferir se já cobre isso ou se é mais um item da mesma lista antes de duplicar trabalho). Investigado a fundo (dois backends de referência):
- Classic GL (`glc_surf.c`): wireframe simples, `GL_LINE_LOOP` por polígono.
- Modern GL (`glm_rsurf.c`, `gl_framebuffer.c`): post-processing de verdade — segundo color attachment (`fbtex_worldnormals`, RGBA16F) escrito pelo shader do mundo (normal + depth linearizada, com quebra forçada entre tipos de turb), depois edge-detect num passo de pós-processo separado.

Consultado o Fable 5 sobre arquitetura Vulkan para isso. Recomendações principais:
1. Sem `VK_KHR_dynamic_rendering` — ficar em render pass clássico (apiVersion real no código é 1.0).
2. Attachment de normais **sempre alocado**, mesmo com outline desligado — gatear só a escrita via push constant (mesmo padrão do `pc.fxaaEnabled` no post-process), nunca duplicar pipelines.
3. Reusar `push.surfaceType` já existente, mas validar granularidade dos subtipos de turb antes de assumir 1:1 com o GL.
4. Estender `vk_post_process.frag` existente com um segundo binding, em vez de shader separado.
5. **Maior risco técnico**: MSAA + attachment de normais — todos os attachments de um subpass precisam da mesma contagem de samples; normais devem ser sempre single-sample, o que pode forçar mover pra subpass separado se MSAA estiver ativo. Resolver esse ponto de design antes de escrever qualquer código.

Outros cuidados: layout transition write→read do novo attachment (mesma barreira que já existe pra `sceneColor`, cuidando dos 2 frames em voo), formato RGBA16F pode ser caro em GPU mobile (considerar normal octaédrica `R16G16_SFLOAT`), `LOAD_OP_CLEAR` explícito pra não herdar lixo de tile em superfícies que pulam esse pipeline.

**Não iniciado ainda** — Tiago decidiu focar em performance primeiro. Retomar quando ele quiser.

### Regras e lições (sessão 2026-07-22)

- Nunca commitar/push sem autorização explícita do Tiago no momento da conversa atual.
- Build Windows deste worktree: junction `C:\eqspr`, script `_build_wip.bat`. Build gerado em `build-msvc-x64/RelWithDebInfo/ezquake.exe`, copiar para `C:\ezquake\ezquake.exe`. **Antes de copiar, sempre fechar o `ezquake.exe` em execução** (`Stop-Process -Force`) ou o `cp` falha com "Device or resource busy".
- **Cuidado com `continue.md`/`CONTINUE.md` em filesystem case-insensitive (Windows)**: são o MESMO arquivo físico. Um `git rebase`/`checkout` que troca entre um commit com `continue.md` e outro com `CONTINUE.md` pode sobrescrever silenciosamente o conteúdo sem conflito aparente. Sempre usar um só (este, maiúsculo, já que é o que está commitado) e nunca criar a variante minúscula à parte.
- Fable 5 é útil para segunda opinião de arquitetura/performance — mas sempre peça pra ele investigar o código real (Explore/leitura de arquivos), não opinar em abstrato, e sempre valide as conclusões dele contra o código antes de agir.
- Duas branches/worktrees irmãs, mesmo renderer Vulkan (`vk_*.c`): **Desktop** `E:\Projetos Linux\ezquake-sdl3-vulkan-pr` (branch `feature/sdl3-vulkan-pr`, PR #1145, regra inegociável: nada Android pode vazar pra cá) e **Android** `E:\Projetos Linux\ezquake-source` (branch `feature/android-pocket-vulkan`, sem PR ainda). Trabalho de Vulkan validado no desktop deve, quando não quebrar o Android, ser portado para os dois.
- **Este projeto tem pelo menos 3 frentes de trabalho concorrentes**: Codex rodando em Linux (`/home/tiago/...`, ver histórico abaixo), Claude rodando em Windows (`E:\Projetos Linux\...`), e o próprio Tiago. Sempre `git fetch`/checar `git log HEAD..origin/<branch>` antes de push — presumir que o remoto não mudou é o tipo de erro que quase causou perda de trabalho nesta sessão.

## Sessão 2026-07-05 (Codex, Linux) — histórico anterior, mantido como referência

Atualizado em: 2026-07-05 (sessão longa, muita coisa mudou — leia isso todo antes de continuar)

## Contexto geral

- Repo "canônico" limpo: `/home/tiago/ezquake-source` (branch `feature/sdl3-vulkan-pr`,
  sempre em sync com `origin`).
- Repo de trabalho ativo: `/home/tiago/projetoslinux/ezquake-source` (mesma branch) —
  é aqui que ficam as mudanças locais, ainda **não commitadas**.
- PR aberta: https://github.com/QW-Group/ezquake-source/pull/1145
  "Add Vulkan renderer backend and migrate desktop client to SDL3".
- Binário de teste: `/home/tiago/nquake/ezquake-vulkan-test` (copiado manualmente do
  build a cada mudança — não esquecer de recompilar + copiar antes de testar).
- Pra testar: `cd /home/tiago/nquake && DISPLAY=:0 ./ezquake-vulkan-test -condebug`,
  roda visível na tela real do usuário (não é headless). Log vai pra
  `qw/qconsole.log`. **Rodar o jogo via script/automação sem o usuário sentado na
  tela não é confiável** — o jogo parece não renderizar frames reais sem foco de
  janela, os screenshots automáticos saem pretos mesmo sem bug nenhum. Pra
  screenshot de verdade, ou pede pro usuário tirar (`screenshot` no console) ou
  aceita que só serve pra ver o console log, não pra validar visual.

## Bugs corrigidos nesta sessão (commitar em breve — ainda sem commit)

Todos os arquivos abaixo têm diff pendente em `projetoslinux/ezquake-source`,
nada commitado ainda. Revisar e limpar antes de commitar (tem debug logging do
Fable de uma sessão anterior misturado, ver seção seguinte).

1. **`vid_restart` reregistrando cvars/comandos toda vez que reinicia** (spam de
   "Can't register variable X, already defined" no console, achado real, não
   afeta visual mas é trabalho desnecessário a cada restart — candidato a
   contribuir pra instabilidade de FPS reportada). Faltava guard
   `if (!host_initialized)` em:
   - `src/vx_tracker.c` `InitTracker()` — tinha um guard **invertido**
     (`if (!qmb_initialized) return;`), que na real só controlava se
     texturas/coronas deviam recarregar, não se cvars deviam re-registrar.
     Corrigido: `if (host_initialized) return;` logo após o carregamento de
     texturas/coronas (que continuam rodando sempre), antes do bloco de
     `Cvar_Register`.
   - `src/cl_screen.c` `SCR_RegisterDamageIndicatorCvars()` — não tinha guard
     nenhum. Corrigido com early-return.
   - `src/sbar.c` `Sbar_Init()` — só o bloco de cvars/comandos
     (`scr_scoreboard_*`, `+/-showscores`, `+/-showteamscores`) ficou dentro do
     guard; o carregamento dos WAD pics e `CL_LoginImageLoad` continuam sempre.
   - `src/hud_editor.c` `HUD_Editor_Init()` — só o `Cmd_AddCommand("hud_editor")`
     + 4 cvars `hud_editor_allow*` ficaram dentro do guard; carregamento dos
     ícones de cursor continua sempre.
   Verificado: rodando `map dm3` + preset Fastest + `vid_restart` com
   `-condebug`, o spam de "already defined" foi de ~130 linhas pra zero.

2. **Preset "Fastest" com cores erradas no `r_drawflat` (Vulkan)** — CONFIRMADO
   visualmente e CORRIGIDO. Causa raiz: `VK_WorldFlatColorForSurface`
   (`src/vk_world.c`) usava a cor média da textura (`texture->flatcolor3ub`)
   pra superfícies normais de parede/chão, ao invés de `r_wallcolor`/
   `r_floorcolor`. Corrigido pra usar os cvars (igual o
   `GLC_SurfFlatColor` faz em `src/glc_brushmodel.c:195-213`).

3. **`r_drawflat` sem sombreamento/lightmap no Vulkan** (ficava "chapado",
   sem gradiente de luz que o GL mostra) — CONFIRMADO e CORRIGIDO. O pipeline
   `vk_world_flat.vert`/`.frag` nunca tinha textura de lightmap conectada.
   Adicionado:
   - atributo `lightmap_coords` no vertex shader (já existia no VBO
     compartilhado, só faltava expor).
   - sampler de lightmap (set=1, reaproveitando `VK_TextureDescriptorSetLayout()`
     genérico) no fragment shader.
   - novo campo `drawflatColor` no push constant (`vk_world_push_t` e nos
     shaders) pra distinguir "isso é r_drawflat de verdade" de "textura ainda
     não carregou, mostrando fallback" — só multiplica pela lightmap no caso
     verdadeiro.
   - `vk_world.c`: novo campo `drawflatCvar` em `vk_world_draw_t`, setado em
     `VK_WorldQueueSurface`; render loop agora bind a descriptor set da
     lightmap (com fallback pra `solidwhite_texture`) quando usa o pipeline flat.
   Usuário confirmou via screenshots (`ezquake012.png` vulkan vs `ezquake013.png`
   gl) que ficou "idêntico" depois desse fix.

**Ambos os fixes 2 e 3 ainda têm o diff de debug do Fable misturado em
`src/vk_aliasmodel.c`** (reativa `VK_AliasDebugLog`, log extra em draws de
arma) — isso é só instrumentação, não faz nada sozinho. Decidir se remove antes
de commitar ou mantém atrás de uma cvar de debug.

## Bug NÃO resolvido: fillet vermelho da granada/rocket sumindo no Vulkan

Confirmado visualmente várias vezes (`ezquake014/015.png` vulkan vs
`ezquake016.png` gl, e antes `vk_crop.png` vs `gl1_crop.png`/`gl2_crop.png`):
granada (`progs/grenade.mdl`, no chão) e o projétil da rocket launcher perdem
uma faixa vermelha (fullbright) que aparece normal no GL. **Usuário confirmou
que só trocou o `vid_renderer`, nenhum outro cvar** — ou seja, se aparece no
GL com as configs padrão, tem que aparecer no Vulkan também. Isso invalida a
hipótese de "só funciona com gl_program_aliasmodels 0" — o bug é real e size
está mesmo lá.

Investigação extensa (eu + 2 rounds de agente Fable) não achou a causa raiz por
leitura estática. O que já se sabe:
- `src/r_aliasmodel_skins.c` carrega a textura fullbright (`fb_texnum`) via
  caminho compartilhado (`Img_HasFullbrights` + `R_LoadTexture(...,
  TEX_FULLBRIGHT)`), igual pros 3 renderers.
- `src/r_aliasmodel.c:355-359` lê `paliashdr->glc_fb_texturenum[skin][anim]` e
  passa pro `renderer.DrawAliasFrame(...)` igual pros 3 renderers — sem
  bifurcação por renderer nesse ponto.
- `R_OverrideModelTextures` (`r_aliasmodel.c:277-279`) só zera `fb_texture` se
  `ent->full_light || !gl_fb_models.integer` — `gl_fb_models` é `1` por
  default, e o ruleset de multiplayer (`Rulesets_FullbrightModel`) força
  ambientlight/shadelight mas NÃO seta `full_light=true`, então `fb_texture`
  não deveria ser zerado no caso comum.
- `src/vk_aliasmodel.c` `VK_AliasQueueFullbrightDraw`/`VK_DrawAliasFrame`
  replica estruturalmente o que o GL clássico legado
  (`gl_program_aliasmodels 0`, não é o default) faz: pass base + pass extra
  alpha-blend com a fb_texture. Pela leitura, parece correto.
- Tanto Modern GL (`glm_aliasmodel.c:435-449`, `GLM_DrawAliasFrame` descarta o
  parâmetro `fb_texture` completamente) quanto Classic GL com
  `gl_program_aliasmodels 1` (default, `glc_aliasmodel.c:361-415`,
  `GLC_DrawAliasFrameImpl_Program` usa a texture unit 1 pra caustics
  subaquático, não pra fb_texture) **também não desenham esse overlay**. Então
  a teoria de "overlay separado que o Vulkan não desenha direito" ficou capenga
  — se nem o GL default desenha um overlay separado, e mesmo assim a faixa
  aparece lá, então a faixa provavelmente é parte da **textura base** do
  modelo (não um overlay), mostrada "crua"/sem sombra quando
  `Rulesets_FullbrightModel` força brilho total — e o bug real estaria no
  **pass base do Vulkan** (`VK_AliasQueueDraw`, `src/vk_aliasmodel.c:606-648`),
  não no `VK_AliasQueueFullbrightDraw`.

**Próximo passo recomendado** (ainda não feito): parar de ler código e
instrumentar de verdade. Estender `VK_AliasDebugLog` (hoje só cobre
`draw->weapon`, não cobre entidades de mundo tipo granada/rocket) pra logar
`R_AliasModelColor`/textura escolhida no pass base pra esses modelos
especificamente, e comparar com um log equivalente no GL (`Con_Printf`
temporário em `glc_aliasmodel.c`/`glm_aliasmodel.c`) rodando exatamente a
mesma cena. Precisa do usuário sentado na tela pra reproduzir (screenshot
automatizado não presta, ver aviso lá em cima).

## Auditoria completa GLM → Vulkan (Fable, 2026-07-05)

Pedido explícito do usuário: implementar essa lista **uma por uma, da mais
difícil pra mais fácil**. Ordem de trabalho:

1. [x] **IMPLEMENTADO nesta sessão, ainda não testado ao vivo.** Fog inexistente no Vulkan (grande) — nenhum `r_fx_fog*` é lido em
   nenhum shader Vulkan. GLM injeta fog globalmente via `#define DRAW_FOG` em
   `src/gl_program.c:521`, `applyFog`/`applyFogBlend` (`:1578-1636`), uniforms
   `fogDensity`/`fogColor` de `src/glm_misc.c:161`. Usado em
   `draw_world.fragment.glsl:169,205,214,264`, `draw_aliasmodel.fragment.glsl`,
   `draw_sprites.fragment.glsl:22`. Precisa adicionar fog aos push constants +
   cálculo em todos os fragment shaders Vulkan (mundo, alias, sprite, flat).

   **Como foi implementado**: `r_refdef2.fog_*` já é populado todo frame por
   código compartilhado (`R_ConfigureFog`, `src/r_rmain.c:314`) a partir dos
   cvars `r_fx_fog*` — Vulkan só precisava LER, não reimplementar parsing de
   cvar. Adicionado aos push constants de `vk_world_push_t` (+16 bytes:
   fogColor vec4 + fogDensity/fogLinearStart/fogLinearEnd/fogCalculation,
   struct foi de 144→160 bytes), `vk_alias_push_t` (124→160 bytes) e
   `vk_sprite3d_push_t` (160→176 bytes). Fórmulas linear/exp/exp2 replicadas
   em GLSL em cada `.frag` (`vk_world_textured.frag`, `vk_world_lightmapped.frag`,
   `vk_world_alpha_textured.frag`, `vk_world_flat.frag`, `vk_alias_model.frag`,
   `vk_sprite3d.frag`). **Diferença de abordagem vs. GL**: ao invés de
   replicar `gl_FragCoord.z/w` (depende da convenção de clip-space do GL, que
   diverge da do Vulkan), uso distância real até a câmera
   (`length(worldPos - cameraPosition)`) — visualmente equivalente, mais
   simples, não depende de reverter a conversão de depth range que os vertex
   shaders já fazem (`clip.z = clip.z * 0.5 + clip.w * 0.5`). Pra mundo e
   sprites é por-fragmento (varying interpolado); pra alias models é um
   único escalar por entidade calculado na CPU (`VectorDistance(ent->origin,
   r_refdef.vieworg)`, `src/vk_aliasmodel.c` em `VK_AliasQueuePreparedDraw`)
   já que os modelos são pequenos o suficiente pra não fazer diferença visual
   por-pixel. Céu (`vk_world_flat.frag`) usa blend constante com
   `r_fx_fog_sky` (guardado no canal alfa não usado de `fogColor`) ao invés
   de fog por profundidade, já que o céu é "infinitamente longe".

   **NÃO TESTADO AO VIVO AINDA** — compilou limpo, mas preciso confirmar
   visualmente. Pra testar: `r_fx_fog 1` (ou `2` = só debaixo d'água),
   `r_fx_fog_density` (default 0.125), `r_fx_fog_start`/`r_fx_fog_end` (fog
   linear), `r_fx_fog_sky` (quanto o céu pega fog, default 0.3) — ou entrar
   num mapa/servidor que force fog (`cl.map_fog_density` via server, tipo
   alguns mapas customizados). Comparar com GL no mesmo cenário.

2. [ ] **REVERTIDO/DESATIVADO — precisa redesign, não tentar de novo sem
   isso.** Outlines de mundo (`gl_outline & 2`). Testado ao vivo com o
   usuário e causou vários bugs sérios (ver "O que deu errado" abaixo). Todo
   o código da tentativa continua no repo, só desativado via
   `VK_WorldNormalsAttachmentActive()` (`src/vk_renderpass.c`) sempre
   retornando `false` — não apagar essa implementação, só não reativar sem
   redesenhar a parte de sincronização/lifetime dos recursos primeiro.

   **O que deu errado (testado ao vivo, várias rodadas)**:
   1. Tela piscando tipo "night club" ao ligar `gl_outline 2`. Causa #1:
      `worldNormalsImage` era um recurso ÚNICO compartilhado entre todos os
      swapchain images (copiei o padrão de depthImage/msaaColorImage, que
      são só render target), mas esse é lido pelo pós-processo no MESMO
      frame em que é escrito — com múltiplos frames em voo, uma escrita
      pisava em cima da leitura. Corrigido virando array por-imagem.
      Piscar continuou.
   2. Faltava barreira de sincronização explícita entre a passada principal
      (escreve worldNormals) e o pós-processo (lê worldNormals) dentro do
      MESMO frame — adicionada em `VK_PostProcessTransitionForSampling`
      (`vk_draw.c`). Ajudou mas não resolveu tudo.
   3. Instalado `vulkan-validation-layers` (não vinha instalado no sistema)
      pra conseguir diagnóstico real com `-dev`. Achado:
      **`VkDescriptorSet ... was destroyed or updated without
      UPDATE_AFTER_BIND`** — um descriptor set sendo destruído enquanto um
      command buffer que ainda o referencia (mesmo que não tenha sido
      submetido ainda) é invalidado por spec. Rastreado até
      `VK_DestroyPostProcessResources`/`VK_WorldResourcesShutdown`
      destruindo pools de descriptor sem esperar a GPU
      (`vkDeviceWaitIdle` adicionado em `VK_DestroyPostProcessResources`,
      ajudou mas não eliminou 100% — o problema real é que
      `vkDeviceWaitIdle` sozinho não protege um command buffer que JÁ
      COMEÇOU a ser gravado no mesmo frame em que o recurso é destruído;
      precisa garantir que a recriação só acontece numa fronteira de frame
      bem definida, não em qualquer evento tipo vid_restart no meio do
      caminho).
   4. Efeito colateral concreto do bug: texturas com partes pretas
      (normalmente onde teria sombra), HUD "vazando" o mapa por trás ao dar
      tab (blend state corrompido), e um bug LATENTE pré-existente do
      screenshot Vulkan exposto de brinde (a imagem do swapchain nunca teve
      a flag `VK_IMAGE_USAGE_TRANSFER_SRC_BIT` que a cópia do screenshot
      exige — só nunca deu erro porque o driver AMD/RADV é tolerante; não
      tem relação com o outline, só ficou visível com a validação ligada).
   5. Também confirmado (não é bug): outline de mundo só aparece com
      `sv_cheats 1` + ruleset default —
      `RuleSets_DisallowModelOutline(NULL)` (`src/rulesets.c:95-99`) já
      bloqueia isso por design em qualquer renderer, bate com o texto de
      ajuda do próprio `gl_outline`.

   **Lição pra próxima tentativa**: a abordagem "forçar o pós-processo
   sempre ativo + redesenhar o mundo de novo numa pipeline dedicada" tem
   efeitos colaterais grandes demais pra small trial-and-error ao vivo.
   Repensar: (a) recriação de descriptor pools/framebuffers deveria
   acontecer só entre frames, com fence/wait garantido antes de qualquer
   novo command buffer começar a gravar; (b) considerar não forçar
   `VK_PostProcessActive()` e em vez disso ter uma passada de composição
   MENOR e dedicada só pro outline, sem depender do pipeline de
   gamma/contrast/FXAA; (c) testar cada mudança pequena com `-dev` +
   validação ligada desde o início dessa vez (agora já está instalado).

   ---

   Escrita original da tentativa (mantida como referência técnica):

   GLM tem passe de geometria (`DRAW_GEOMETRY`,
   `src/glm_rsurf.c:151,212-214`, saída `normal_texture`) + pós-passe
   `src/glsl/fx_world_geometry.fragment.glsl` (`GLM_DrawWorldOutlines`,
   `src/glm_rmain.c:63,131`).

   **Como foi implementado (bem diferente do GLM, ver histórico da conversa
   pra entender por que)**: o usuário pediu explicitamente pra fazer via MRT
   (múltiplos render targets, igual o GLM), mas o SPIR-V do Vulkan aqui é
   pré-compilado em build-time (não runtime como o GLSL do GLM), então não
   dá pra ter "variantes" de shader condicionais por `#ifdef` fácil. Solução:
   em vez de fazer TODOS os shaders de mundo escreverem num segundo
   attachment condicionalmente, criei um **pipeline dedicado e uma passada
   extra**:
   - `src/vk_local.h` + `src/vk_swapchain.c`: nova imagem
     `worldNormalsImage`/View (`VK_FORMAT_R8G8B8A8_UNORM`, single-sample,
     compartilhada entre swapchain images como o depth buffer). **Só existe
     quando MSAA está desligado** (`VK_WorldNormalsAttachmentActive()` em
     `vk_renderpass.c`) — misturar attachment single-sample num subpass
     multisample não é portável em Vulkan core sem extensão, e não valia a
     complexidade de um resolve extra só pra isso. Ou seja: **outline não
     funciona com MSAA ligado**, cai silenciosamente pra sem-outline (não é
     bug, é limitação documentada).
   - `src/vk_renderpass.c`: `vk_renderpass_main`/`_noclear` agora sempre tem
     3 attachments — com MSAA é (msaaColor, depth, resolve) como antes; sem
     MSAA é (color, depth, worldNormals). `VK_WorldNormalsAttachmentActive()`
     exposta em `vk_local.h` pra todo mundo que precisa saber quantos
     color-blend-attachments declarar.
   - **Todo pipeline que desenha nessa render pass precisou de um 2º
     color-blend-attachment** pra bater com o subpass (senão é erro de
     validação/pipeline inválido): mundo (4 pipelines em `vk_world.c`), alias
     models (`vk_aliasmodel.c`), sprites (`vk_sprite3d.c`), HUD/2D
     (`vk_draw.c`, via `VK_BlendingConfigure` que ganhou um parâmetro
     `mainRenderPass` novo). Em todos esses o 2º attachment é
     **write-disabled** (`colorWriteMask=0`, não escreve nada) — nenhum
     deles escreve normal de verdade.
   - `src/vulkan_shaders/vk_world_normals.vert` + `.frag` (novos, registrados
     em `CMakeLists.txt`): pipeline dedicado (`VK_WorldCreateNormalsPipeline`
     em `vk_world.c`) que **redesenha a geometria opaca do mundo já visível
     uma segunda vez**, só com atributo de posição (reaproveita
     `vbo_world_vert_t`), depth-test igual ao das outras (LEQUAL/GEQUAL) mas
     `depthWriteEnable=false` (não escreve depth de novo, só usa o que já
     foi escrito pra saber o que é frontmost), sem descriptor sets (só push
     constant com o mvp). O fragment shader calcula a normal via
     `dFdx`/`dFdy` da posição no mundo (sem precisar de normal por-vértice
     no VBO) e escreve `vec4(normal*0.5+0.5, gl_FragCoord.z)` no attachment 1.
     Chamado num loop novo em `VK_RenderView` (`vk_world.c`), logo depois do
     loop principal, iterando `worldDraws[]` e pulando os `blended` (só
     opacos contam pro silhouette).
   - Composição final: **não** um passe de render separado — estendi o
     shader de pós-processo que já existe (`vk_post_process.frag`,
     `ApplyWorldOutline`), que agora também amostra `worldNormals` (binding 1
     novo) e compara os 4 vizinhos (`texelFetch`) pra detectar silhueta
     (produto escalar de normais) e crista (2ª derivada de profundidade,
     como o GLC faz). Push constant novo em `vk_post_process_push_t`
     (`vk_draw.c`): `outlineColor` (rgb dos cvars + alpha como flag
     liga/desliga), `outlineDepthThreshold`, `outlineNormalThreshold`,
     `outlineScale`.
   - **IMPORTANTE**: `VK_PostProcessActive()` (`vk_swapchain.c`) precisou
     ganhar um `R_DrawWorldOutlines() && VK_WorldNormalsAttachmentActive()`
     no gate — sem isso, o passe de composição inteiro (onde o outline é
     desenhado) só rodava quando gamma/contraste/FXAA já estavam ativos, e o
     outline ficaria morto em silêncio com configs padrão.
   - **Discrepância conhecida de calibração**: o GLC/GLM usam profundidade
     LINEAR (`r_zFar * diff`) pro teste de crista; aqui uso
     `gl_FragCoord.z` do Vulkan, que é profundidade NÃO-linear (device
     depth). Isso significa `gl_outline_world_depth_threshold` (default "4")
     se comporta numa escala bem diferente — na prática, a detecção de
     crista provavelmente não dispara com o valor default (silhueta via
     normal ainda funciona normal). Precisa recalibrar esse cvar
     especificamente pro Vulkan, ou aceitar que só a silhueta funciona por
     enquanto.

   **BUG achado e corrigido ao vivo**: primeira versão fazia a tela inteira
   piscar tipo "night club" assim que `gl_outline 2` era ligado. Causa:
   `worldNormalsImage`/View eram um recurso ÚNICO compartilhado entre todos
   os swapchain images (copiei o padrão de `depthImage`/`msaaColorImage`,
   que são "nunca amostrados, só render target" — mas o world-normals É
   amostrado pelo post-process no MESMO frame em que é escrito). Com
   `VK_MAX_FRAMES_IN_FLIGHT > 1`, o frame N+1 podia começar a escrever nessa
   imagem enquanto o frame N ainda estava lendo ela no passe de composição —
   race condition clássica de GPU, gerando aquele "strobing". Corrigido
   convertendo pra **um array por swapchain image** (`worldNormalsImages[]`/
   `worldNormalsImageMemory[]`/`worldNormalsImageViews[]` em `vk_local.h`),
   exatamente como `postProcessColorImages[]` já fazia (o comentário
   original dele já explicava esse hazard — eu só não apliquei a mesma
   lição na hora de criar o recurso novo). Mexeu em `vk_swapchain.c`
   (criação/destruição agora em loop, uma por imagem) e `vk_draw.c`
   (descriptor set usa `worldNormalsImageViews[imageIndex]`).

   **Testado**: compilação limpa (build incremental E do zero), roda sem
   crash/sem erro no console com `-condebug` (startup+quit, e
   `map dm3` + `gl_outline 2` + 60 frames + quit) — inclusive depois do fix
   do piscar. **Ainda não confirmado se o outline aparece visualmente
   correto** (o usuário só confirmou que o piscar sumiu, não testou a
   qualidade/calibração do efeito em si ainda).
3. [ ] **Pós-processo incompleto** (grande) — sem tonemap HDR
   (`EZ_POSTPROCESS_TONEMAP`, `src/glm_framebuffer.c:91,104-105`), sem
   framebuffer separado 3D/HUD (`glm_framebuffer.c:90,102`),
   `VK_FramebufferCreate` retorna false, `R_SUPPORT_FRAMEBUFFERS` não é
   anunciado (`vk_main.c:1002-1006,1054`) — `vid_framebuffer*`/supersampling
   não existem. FXAA é aproximação de 4 taps, não o FXAA 3.11 real
   (`vk_post_process.frag:18-23`, comentário já admite).
4. [ ] **Caustics subaquáticos (`gl_caustics`) inexistentes** (médio) — GLM em
   `src/glm_rsurf.c:133,141,167-172,304` (mundo) e
   `src/glm_aliasmodel.c:154,163-168,424,535` (modelos). Vulkan descarta
   explicitamente: `src/vk_world.c:1683` (`(void)caustics;`). Precisa sampler
   extra + mix no fragment dos pipelines lightmapped/textured e alias.
5. [ ] **Modelos alias sem iluminação direcional por vértice** (médio) — GLM
   calcula por normal do vértice (`draw_aliasmodel.vertex.glsl:66-77`). Vulkan
   usa escalar único por modelo (`src/vk_aliasmodel.c:628`). Deixa
   jogadores/itens/armas com brilho uniforme, sem volume. Precisa passar
   `shadelight`/`ambientlight`/`yaw_angle_rad` nos push constants e replicar a
   fórmula no vertex shader.
6. [ ] **`r_drawflat_mode` 1 e 2 (tinted/bright) não implementados** (médio) —
   GLM suporta via `DRAW_DRAWFLAT_TINTED`/`DRAW_DRAWFLAT_BRIGHT`
   (`src/glm_rsurf.c:145-147`, `draw_world.fragment.glsl:56-86`). Vulkan só
   funciona com `r_drawflat_mode 0` (`src/vk_world.c:1639-1640` — número de
   linha pode ter mudado depois do fix do item 2/3 acima, conferir). Com modo
   1/2 o Vulkan mostra o mundo texturizado normal, como se drawflat estivesse
   desligado.
7. [ ] **Luma de mundo não modulada pela lightmap** (médio) — GLM soma luma
   antes de multiplicar pela lightmap quando `gl_fb_bmodels 0`
   (`draw_world.fragment.glsl:241-245`) e faz decal quando `gl_fb_bmodels 1`
   (`:246-254`). Vulkan desenha luma como segundo passe aditivo depois da base
   já iluminada (`src/vk_world.c` `VK_WorldCreateOverlayPipeline` ~linha 956,
   dispatch ~1724-1755) — luma sempre brilha 100% mesmo no escuro, e a
   diferença `gl_fb_bmodels 0/1` desaparece.
8. [ ] **Água iluminada (lit turb) ausente** (pequeno/médio) — GLM marca
   `texture->isLitTurb` (`src/glm_rsurf.c:365`) e multiplica lightmap na turb.
   Vulkan exclui todo `SURF_DRAWTURB` da lightmap
   (`src/vk_world.c:482-484` `VK_WorldLightmapTextureForSurface`).
9. [ ] **Skywind (`r_skywind` + `*_wind.cfg`) ausente** (pequeno/médio) — GLM
   `DRAW_SKYWIND` (`src/glm_rsurf.c:137,153,198-200`;
   `draw_world.fragment.glsl:179-192`; dados em
   `src/r_brushmodel_sky.c:40-49,120`). Nada equivalente em
   `vk_world_flat.frag`.
10. [ ] **`r_lerpmuzzlehack` ignorado** (pequeno) — GLM tem
    `EZQ_ALIASMODEL_MUZZLEHACK` (`src/glm_aliasmodel.c:155,180-181`;
    `draw_aliasmodel.vertex.glsl:54-56`). VBO Vulkan grava o flag
    (`src/vk_renderer_stubs.c:298`) mas `vk_alias_model.vert` não tem atributo
    de flags e lerpa tudo sempre — viewmodel "estica" durante muzzleflash.
11. [ ] **Skybox com clamp fixo em 512px** (pequeno) — `vk_world_flat.frag:84`
    assume face 512×512. GLM usa `samplerCube` real. Skyboxes com resolução
    diferente têm costuras.
12. [ ] **Outline de jogador sem separar cor de cima/baixo** (pequeno) — GLM
    pinta pernas com `bottomcolor` por-fragmento
    (`draw_aliasmodel.fragment.glsl:30-56`). Vulkan só escolhe `topcolor` na
    CPU (`src/vk_aliasmodel.c:452-478`).
13. [ ] **`gl_textureless` vaza pra brush models** (pequeno) — GLM restringe
    ao mundo (`Flags & EZQ_SURFACE_WORLD`,
    `draw_world.vertex.glsl:110-113`). Vulkan seta pra todo draw
    (`src/vk_world.c:1916`, número de linha pode ter mudado).
14. [ ] **`r_dynamic 2` (compute shader) sem equivalente** — SEM IMPACTO
    VISUAL (cai pro caminho software que já funciona). Fable marcou como
    "aceitável documentar", não precisa implementar de verdade — grande
    esforço pra zero ganho visual. Deixar de fora da lista de trabalho, só
    documentado aqui.

Itens verificados como OK (não são gaps, não mexer): powerup shells, r_shadows
(Vulkan até implementa melhor que GLM), dynamic lights em lightmaps,
polyblend/cshifts, fastturb/fastsky com cores dos cvars, detail textures, HUD,
wateralpha.

## Lembrete de processo

- Sempre recompilar (`cmake --build build -j$(nproc)` dentro de
  `projetoslinux/ezquake-source`) E copiar o binário
  (`cp build/ezquake-linux-x86_64 /home/tiago/nquake/ezquake-vulkan-test`)
  antes de pedir pro usuário testar — fácil esquecer o `cp` e o usuário testar
  o binário velho.
- Matar o processo antigo antes de subir um novo
  (`pgrep -fa ezquake-vulkan-test`, `kill -TERM`/`-KILL` se não morrer).
- Não editei/commitei nada via `git commit` ainda nesta sessão — tudo é diff
  local em `projetoslinux/ezquake-source`. Perguntar antes de commitar.
- `vulkan-validation-layers` foi instalado no sistema nesta sessão (Arch/
  CachyOS, pacote `vulkan-validation-layers`). Sempre testar com `-dev
  -condebug` e checar `qw/qconsole.log` por `VUID`/"invalid state"/
  "destroyed" antes de considerar uma mudança Vulkan pronta — não só
  compilar, rodar de verdade com validação.

## REVERT TOTAL desta sessão (2026-07-05)

Depois de uma tentativa de implementar outlines de mundo via MRT causar
piscamento de tela, texturas pretas e corrupção de HUD ao vivo (ver histórico
completo do item 2 na auditoria acima), o usuário pediu revert total pro
estado de antes de qualquer trabalho do Fable (fog + outlines). Feito via
`git checkout -- <arquivo>` nos arquivos que só tinham fog/outline, e reescrita
manual em `vk_world.c`/`vk_world_flat.frag` (que misturavam fog+outline com o
fix de drawflat/lightmap pré-Fable) partindo do `git show HEAD:...` original.
Estado atual = só os fixes pré-Fable (cvars, drawflat cor/lightmap), fog e
outline **zerados**, precisam ser refeitos do zero quando retomar a auditoria.
Confirmado pelo usuário: "parece ter corrigido, esta igual gl e vulkan".

**Bug pré-existente descoberto durante o revert, NÃO introduzido nesta sessão
nem pelo Fable** — continua acontecendo mesmo no estado 100% revertido:
- `VkDescriptorSet ... was destroyed or updated without UPDATE_AFTER_BIND` +
- `vkQueueSubmit(): pSubmits[0].pSignalSemaphores[0] ... may still be in use
  by VkSwapchainKHR` (semáforo reusado antes de ser reapresentado).

Causa provável raiz de ambos: em `src/vk_main.c`, `imageAvailableSemaphores`/
`renderFinishedSemaphores` são arrays indexados por **frame-in-flight**
(`frameIndex`, cicla 0..VK_MAX_FRAMES_IN_FLIGHT-1), mas deveriam ser
indexados pela **imagem do swapchain** (`imageIndex`) — prática recomendada
pela própria doc do Vulkan (linkada no erro:
https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html). Quando
`VK_MAX_FRAMES_IN_FLIGHT != imageCount`, os dois contadores dessincronizam
com o tempo, o semáforo pode ser reaproveitado enquanto ainda em uso pela
apresentação de uma imagem diferente — e a CPU pode achar que pode reusar
recursos (descriptor sets) de um frame antes da GPU/apresentação realmente
terminar, explicando o outro erro também.

**Usuário decidiu NÃO corrigir agora** (perguntei explicitamente, ver
[[feedback-vulkan-incremental-testing]] na memória) — deixar documentado pra
uma sessão futura dedicada a isso, separada da auditoria do Fable. Fix
esperado: trocar os arrays de semáforo pra serem indexados por `imageCount`
(um semáforo por imagem do swapchain) em vez de por frame-in-flight, seguindo
o padrão recomendado.
