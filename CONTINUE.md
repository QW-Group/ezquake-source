# Onde paramos — Vulkan renderer / SDL3 port

Atualizado em: 2026-07-22 (sessão Claude, Linux, `tiba@tiba-System-Product-Name`) — ver seção "Sessão 2026-07-22 (Claude, Linux)" logo abaixo para o estado mais recente. A seção "Sessão 2026-07-22 (Claude, Windows)" e o restante do arquivo continuam válidos como histórico.

## Regra permanente (Tiago pediu explicitamente, sessão 2026-07-22)

Manter este arquivo (`CONTINUE.md`, maiúsculo — é o mesmo slot de arquivo que `continue.md` em filesystems case-insensitive como Windows, não criar um `continue.md` separado) atualizado sempre que uma sessão avançar ou pausar, tanto aqui quanto em `E:\Projetos Linux\ezquake-source\continue.md` (worktree Android, esse sim minúsculo, filesystem diferente/caso não colide lá). Objetivo: qualquer sessão futura (Claude ou Codex, Windows ou Linux) sabe onde o trabalho parou.

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
