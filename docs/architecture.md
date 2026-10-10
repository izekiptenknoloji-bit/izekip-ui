# İzekip GUI — Mimari (Phase 1)

Qt'nin öğrenme eğrisine alternatif: arayüz **stili** gerçek CSS söz dizimiyle, C++ içinde kurulan bir
widget ağacına uygulanır (Qt Style Sheets mantığına benzer, kendi render motoruyla).

## Katmanlar

- **Core** (`include/Core`, `src/Core`): `Color`, `Rect`, `StyleProperties`, `Widget`. `Widget` her
  elemanın layout'unu [Yoga](https://github.com/facebook/yoga) (flexbox motoru) ile hesaplar — her
  widget bir `YGNodeRef` sarar.
- **Widgets** (`include/Widgets`, `src/Widgets`): `Panel`, `Button` — Phase 1'de sadece `Widget`'ın
  tip adını (CSS type selector eşleşmesi için) ayarlayan ince alt sınıflar.
- **Renderer** (`include/Renderer`, `src/Renderer`): `IRenderer` backend-agnostic arayüzü. İlk
  implementasyon D3D11 (`src/Renderer/D3D11`), Vulkan Phase 3'te eklenecek. Backend seçimi derleme
  zamanında `IZEKIP_RENDER_BACKEND` CMake cache değişkeniyle yapılır.
- **Platform** (`include/Platform`, `src/Platform`): GLFW tabanlı `Window` sarmalayıcısı — pencere/giriş
  katmanı en baştan cross-platform'a hazır, ilk renderer (D3D11) şimdilik Windows'a özel.
- **CSS** (`include/CSS`, `src/CSS`): `CssTokenizer` → `CssParser` → `StyleSheet` → `StyleApplier`.
  Tip/`.class`/`#id` selector'lar desteklenir (combinator yok). `StyleApplier`, her widget için eşleşen
  kuralları specificity'ye göre sıralayıp birleştirir ve aynı anda widget'ın Yoga node'una
  width/height/margin/padding yazar.

## Bağımlılıklar

`third_party/glfw` ve `third_party/yoga` git submodule olarak eklenir. DirectX11 Windows SDK ile gelir,
ayrıca bir şey indirmeye gerek yok.

## Örnek

`examples/01_basic_panel_button` — bir `Panel` içinde bir `Button`, stilleri `assets/style.css`'ten
okunur. Diğer fazlar için bkz. proje planı (`.claude/plans` altında, oturuma özel).
