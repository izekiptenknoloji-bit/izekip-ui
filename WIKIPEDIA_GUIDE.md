# İzekip UI - Wikipedia Yazı Rehberi

**Son Güncelleme**: 2026-10-11  
**Amaç**: İzekip UI'ı Türkçe Wikipedia'ya doğru ve kapsamlı şekilde yazmak  
**Hedef Okuyucu**: Teknik bilgisi olan ve teknik olmayan okuyucular  

---

## 📋 İÇİNDEKİLER

1. [Proje Vizyonu & Amacı](#proje-vizyonu--amacı)
2. [Temel Özellikler](#temel-özellikler)
3. [Teknik Mimarisi](#teknik-mimarisi)
4. [Kod Yapısı & Dosya Organizasyonu](#kod-yapısı--dosya-organizasyonu)
5. [Geliştirme Aşamaları (Phases)](#geliştirme-aşamaları-phases)
6. [Kullanım Örnekleri](#kullanım-örnekleri)
7. [Qt ile Karşılaştırma](#qt-ile-karşılaştırma)
8. [Platform Desteği](#platform-desteği)
9. [Lisans & Katkı](#lisans--katkı)
10. [İleride Planlar](#ileride-planlar)

---

## 🎯 Proje Vizyonu & Amacı

### NE İSTER?

İzekip UI, modern bir C++ GUI (Graphical User Interface) kütüphanesi olup, **Qt'ye daha kolay ve web-friendly bir alternatif** sunmayı hedeflemektedir.

### NEDEN?

```
PROBLEM:
├─ Qt öğrenmesi zor
│  ├─ MOC pre-compiler
│  ├─ Signal/Slot sistemi
│  ├─ CMake entegrasyonu karmaşık
│  └─ Yüz+ widget (karışık)
│
└─ CSS bilen insanları UI geliştirmeden uzaklaştırır

ÇÖZÜM:
├─ C++ + Gerçek CSS
├─ Minimal widget set (Panel, Button, Label, TextArea)
├─ GPU hızlı rendering (D3D11 + Vulkan)
└─ Öğrenmesi kolay, ama güçlü
```

### AMAÇ İFADESİ

> İzekip UI, web geliştiricilerin bildikleri CSS dilini kullanarak, GPU ile hızlı 
> ve güzel arayüzler yapmalarına imkan veren, Qt'den daha basit ve modern 
> bir C++ GUI kütüphanesidir.

---

## ⭐ Temel Özellikler

### 1. **Gerçek CSS Styling**

```
Geleneksel Yaklaşım (Qt):
  ↓
  C++ kodunda Her şeyi dizayn et
  ↓
  QSS (Qt Stylesheet) - tuhaf syntax
  ↓
  Karmaşık

Modern Yaklaşım (İzekip UI):
  ↓
  C++ ile widget ağacı oluştur
  ↓
  CSS dosyasıyla stil ver
  ↓
  Kolay ve tanıdık
```

**Desteklenen CSS**:
- Type selectors: `Button { }`
- Class selectors: `.primary { }`
- ID selectors: `#root { }`
- Descendant combinator: `Panel > Button { }`
- Pseudo-classes: `:hover`, `:active`, `:focus`

### 2. **GPU ile Hızlı Rendering**

```
Widget Ağacı          CSS Dosyası        GPU Shader
    ↓                    ↓                  ↓
  [Panel]    +    style.css      →   RectShader.hlsl
   ├─Button                          (D3D11 | Vulkan)
   └─Label
    ↓
Pixel-Perfect Görüntü
```

- **DirectX 11** (Windows native)
- **Vulkan** (Windows/Linux/Mac ready)
- Aynı pixel output (her iki backend)

### 3. **Flexbox Layout**

Facebook'un **Yoga** motorunu kullanarak responsive design:

```css
Panel {
    flex-direction: row;              /* row, column */
    justify-content: center;          /* flex-start, space-between, ... */
    align-items: center;              /* flex-start, center, stretch */
    padding: 20px;
    margin: 10px;
}
```

### 4. **Smooth Animasyonlar**

```css
Button {
    opacity: 1.0;
    transition: all 300ms ease-in-out;
}

Button:hover {
    opacity: 0.8;
    box-shadow: 0px 6px 18px #00000070;
}
```

Animatable özellikler:
- `background-color`
- `width`, `height`
- `margin`, `padding`
- `opacity`

### 5. **Modern Metin Rendering (SDF)**

```
Traditional Rasterization:
  ↓
Pixelated yazı (zoom'da kötü görünür)

SDF (Signed Distance Field):
  ↓
Smooth, scalable yazı (web gibi)
  ↓
stb_truetype + custom atlas baker
```

---

## 🔧 Teknik Mimarisi

### Stack Özeti

| Bileşen | Seçim | Rol |
|---------|-------|-----|
| **Dil** | C++20 | Modern, performans |
| **Pencere** | GLFW 3.x | Cross-platform windowing |
| **Layout** | Yoga | Facebook flexbox motor |
| **CSS Parser** | lexbor | W3C standardı compliant |
| **Renderer** | D3D11 + Vulkan | GPU'ya komut gönderme |
| **Metin** | stb_truetype | Font rasterization |
| **Build** | CMake 3.21+ | Standard build system |

### Veri Akışı (Pipeline)

```
CSS File            C++ Widget Tree
    ↓                      ↓
    └──→ lexbor ─→ LexborBridge
                        ↓
                   StyleApplier
                        ↓
              Computed StyleProperties
                        ↓
         Yoga (Layout) + Renderer (Paint)
                        ↓
              Widget::OnPaint(*renderer)
                        ↓
         RectDrawData (CPU-side math)
                        ↓
    D3D11Renderer         |         VulkanRenderer
          ↓               |              ↓
      GPU Pipeline    SharedShader   GPU Pipeline
          ↓               |              ↓
        D3D11            HLSL           Vulkan
        Buffer    ←  RectShader.hlsl  →  SPIR-V
          ↓               |              ↓
     Screen Output  ←────────────────→ Screen Output
     (identical)
```

### Render Şaderı

```
RectShader.hlsl (Hem D3D11 hem Vulkan'da çalışır)
├─ Rect drawing (solid color)
├─ Border radius (SDF)
├─ Box shadow (smooth)
├─ Gradient (2-stop)
├─ Backdrop filter (blur)
└─ Text rendering (SDF glyph atlas)
```

---

## 📁 Kod Yapısı & Dosya Organizasyonu

```
İzekip UI/
│
├── include/  ← PUBLIC API (Ne kadar yazılacak onu kontrol eder)
│   ├── Core/
│   │   ├── Widget.h          (Widget base class)
│   │   ├── Color.h           (RGBA color)
│   │   ├── StyleProperties.h (Computed CSS)
│   │   ├── Interpolate.h     (Animation math)
│   │   ├── Easing.h          (Cubic-bezier)
│   │   ├── Transition.h      (Property transitions)
│   │   └── TextBuffer.h      (Multi-line text buffer)
│   │
│   ├── CSS/
│   │   ├── CssTokenizer.h    (Declaration value parsing)
│   │   ├── LexborBridge.h    (lexbor → Widget mapping)
│   │   ├── StyleSheet.h      (CSS rule collection)
│   │   └── StyleApplier.h    (CSS → StyleProperties)
│   │
│   ├── Renderer/
│   │   ├── IRenderer.h       (Abstract GPU interface)
│   │   ├── RectDrawData.h    (CPU-side draw math)
│   │   ├── GlyphAtlas.h      (Font texture)
│   │   └── [D3D11/, Vulkan/] (Private backends)
│   │
│   ├── Platform/
│   │   └── Window.h          (GLFW wrapper)
│   │
│   └── Widgets/
│       ├── Panel.h           (Container)
│       ├── Button.h          (Interactive)
│       ├── Label.h           (Text display)
│       └── TextArea.h        (Editable text)
│
├── src/  ← IMPLEMENTATION
│   ├── Core/
│   │   ├── Widget.cpp
│   │   ├── Interpolate.cpp
│   │   ├── TextBuffer.cpp
│   │   └── ...
│   │
│   ├── CSS/
│   │   ├── LexborBridge.cpp
│   │   ├── StyleApplier.cpp
│   │   ├── StyleSheet.cpp
│   │   └── CssTokenizer.cpp
│   │
│   ├── Renderer/
│   │   ├── RectDrawData.cpp  ← SHARED by both backends
│   │   ├── GlyphAtlas.cpp
│   │   ├── RHI.cpp           (Renderer factory)
│   │   │
│   │   ├── D3D11/
│   │   │   ├── D3D11Renderer.h
│   │   │   └── D3D11Renderer.cpp
│   │   │
│   │   ├── Vulkan/
│   │   │   ├── VulkanRenderer.h
│   │   │   └── VulkanRenderer.cpp
│   │   │
│   │   └── Shaders/
│   │       └── RectShader.hlsl  ← ONE SHADER, TWO COMPILERS
│   │
│   ├── Platform/
│   │   └── Window.cpp
│   │
│   └── Widgets/
│       ├── Panel.cpp
│       ├── Button.cpp
│       ├── Label.cpp
│       └── TextArea.cpp
│
├── third_party/  ← VENDORED DEPS (Complete copies)
│   ├── glfw/        (Window system)
│   ├── yoga/        (Flexbox)
│   ├── lexbor/      (CSS parser)
│   ├── stb/         (stb_truetype.h)
│   └── fonts/       (Inter.ttf, Space Mono.ttf)
│
├── examples/  ← DEMO APPS
│   ├── 01_basic_panel_button/
│   │   ├── CMakeLists.txt
│   │   ├── main.cpp
│   │   └── style.css
│   │
│   ├── 02_text_document/
│   │   └── [text document example]
│   │
│   └── 03_notepad/
│       └── [TextArea example]
│
├── tests/
│   ├── CssParserTests.cpp    (CSS parsing)
│   └── TextBufferTests.cpp   (Text buffer logic)
│
├── cmake/
│   ├── Options.cmake         (IZEKIP_RENDER_BACKEND seçimi)
│   ├── Version.h.in
│   └── IzekipUIConfig.cmake.in
│
├── cli/
│   └── cssfr/   (Project scaffolding tool)
│
├── build/            (← CMake output, D3D11 default)
│   └── IzekipUI.slnx
│
├── build-vulkan/     (← CMake output, Vulkan backend)
│   └── IzekipUI.slnx
│
├── CMakeLists.txt    (Main build)
├── README.md         (Quick start)
├── ARCHITECTURE.md   (Detailed API)
├── CHANGELOG.md      (Release history)
├── COMPATIBILITY.md  (Version promises)
├── LICENSE           (MIT)
└── Doxyfile          (Doc generation)
```

---

## 📈 Geliştirme Aşamaları (Phases)

### Phase 1: MVP (Minimum Viable Product)

```
Amaç: İlk çalışan örnek
Tarih: 2026 başı

├─ GLFW pencere
├─ DirectX 11 device initialization
├─ Hand-rolled CSS parser (basit: type/.class/#id only)
├─ Yoga layout engine integration
└─ Flat-color rect rendering (renkli dikdörtgenler)

Sonuç: examples/01_basic_panel_button çalışıyor
```

### Phase 2: Interactive UI

```
Amaç: Mouse input ve animasyon
Tarih: 2026 erken

├─ GLFW mouse/cursor callbacks
├─ Widget::HitTest (tıkla nereye basıldı?)
├─ Hover state management
├─ :hover ve :active CSS pseudo-classes
├─ CSS transition property
├─ Cubic-bezier easing engine (Newton-Raphson solver)
├─ Smooth color/size/margin animasyonlar
└─ Widget::m_targetStyle vs freshly-resolved cascade

Sonuç: Animated button örneği çalışıyor
```

### Phase 3: Visual Effects

```
Amaç: Güzel görünüm
Tarih: 2026 orta

├─ border-radius (SDF signed-distance-field shader)
├─ box-shadow (gölge efekti)
├─ linear-gradient (geçişli renkler)
├─ Premultiplied alpha blending (transparanslık)
└─ Inigo Quilez SDF algorithms (matematik)

Karar: Bu üç özellik henüz animate edilmiyor (snap-only)

Sonuç: Güzel görünümlü modern UI
```

### Phase 4: Vulkan Backend

```
Amaç: Cross-platform support
Tarih: 2026 orta-sonu

├─ İkinci IRenderer implementasyonu
├─ SPIR-V shader compilation (build-time dxc)
├─ Shared RectShader.hlsl
├─ Vulkan surface creation (GLFW bridge)
├─ Swapchain management
├─ Validation layer testing
├─ Semaphore/fence synchronization
└─ Pixel-identical output (D3D11 ile aynı görüntü)

Donanım test: Intel UHD Graphics + Vulkan 1.4
Sonuç: Her iki backend aynı şekilde çalışıyor
```

### Phase 5: Standards-Compliant CSS

```
Amaç: Gerçek CSS desteği
Tarih: 2026 sonu

├─ Hand-rolled parser'ı lexbor'a geçir
├─ Descendant combinators (Panel > Button)
├─ Multi-value CSS shorthands (margin: 10px 20px 30px 40px)
├─ flex-direction, justify-content, align-items Yoga'ya bağla
├─ StyleApplier'a LexborBridge parametre ekle
└─ Cascade + specificity doğru şekilde

Sonuç: W3C CSS standardlarına uyumlu
```

### Phase 6: Text Rendering (SDF)

```
Amaç: Modern metin gösterimi
Tarih: 2026-10 sonu

├─ stb_truetype glyph rasterization
├─ 512x512 SDF atlas (signed-distance-field texture)
├─ RectShader.hlsl'e text mode flag ekle
├─ Widgets::Label (single-line text)
├─ Font sizing via Yoga's YGNodeSetMeasureFunc
├─ Latin-1 + Turkish characters (ş, ğ, ı, İ, etc.)
├─ D3D11 + Vulkan texture/sampler support
└─ GlyphAtlas::FindGlyph (codepoint lookup)

Sonuç: Türkçe karakterlerle metin gösterebiliyor
```

### Phase 7: TextArea & Keyboard

```
Amaç: Editable text input
Tarih: 2026-11 (DEVAM EDEN)

├─ Widgets::TextArea (multi-line editable)
├─ TextBuffer data structure (insert/delete/move)
├─ Keyboard event handling
├─ Platform::Window::SetKeyCallback / SetCharCallback
├─ :focus pseudo-class
├─ Cursor positioning
└─ examples/03_notepad/

Scope kararı: Text selection/clipboard/undo-redo henüz yok

Sonuç: Basit metin editörü yapılabiliyor
```

### Phase 8+ (Future)

```
Planlanmış ama Başlanmayan:
├─ Checkbox, Select, Slider widgets
├─ Text selection + clipboard + undo/redo
├─ Unicode full support
├─ CSS Grid layout
├─ Absolute positioning + z-index
├─ font-weight, font-family CSS
├─ Percent-based margin/padding
├─ Animation scope cuts (backdrop-filter blur -> erf)
└─ Linux/macOS native testing
```

---

## 💻 Kullanım Örnekleri

### Örnek 1: Basit Buton

**C++ Kodu:**
```cpp
#include "Widgets/Button.h"
#include "Widgets/Panel.h"

// Widget ağacı oluştur
auto root = std::make_shared<Widgets::Panel>();
root->SetId("root");
root->AddClass("container");

auto button = std::make_shared<Widgets::Button>();
button->AddClass("primary");
button->AddClass("large");
root->AddChild(button);
```

**CSS Kodu:**
```css
Panel#root.container {
    background-color: #F5F5F5;
    width: 800px;
    height: 600px;
    padding: 20px;
    flex-direction: column;
    justify-content: center;
    align-items: center;
}

Button.primary {
    background: linear-gradient(to right, #3366FF, #5588FF);
    border-radius: 8px;
    width: 120px;
    height: 40px;
    opacity: 1.0;
    transition: all 300ms ease-in-out;
}

Button.primary:hover {
    opacity: 0.8;
    box-shadow: 0px 6px 18px #00000070;
}

Button.primary:active {
    opacity: 0.6;
}
```

**Sonuç**: Gradyan buton, hover'da sollaşıyor, tıkla daha da sollaşıyor

---

### Örnek 2: Metin Etiketi

**C++ Kodu:**
```cpp
#include "Widgets/Label.h"

auto label = std::make_shared<Widgets::Label>();
label->SetText("Merhaba, İzekip UI!");
label->AddClass("title");
root->AddChild(label);
```

**CSS Kodu:**
```css
Label.title {
    color: #222222;
    font-size: 24px;
    opacity: 1.0;
    transition: opacity 200ms ease-in-out;
}

Label.title:hover {
    opacity: 0.7;
}
```

---

### Örnek 3: Metin Editörü

**C++ Kodu:**
```cpp
#include "Widgets/TextArea.h"

auto editor = std::make_shared<Widgets::TextArea>();
editor->AddClass("editor");
editor->SetFocused(true);
root->AddChild(editor);
```

**CSS Kodu:**
```css
TextArea.editor {
    background-color: #FFFFFF;
    color: #000000;
    width: 100%;
    height: 400px;
    padding: 10px;
    border-radius: 4px;
}

TextArea.editor:focus {
    box-shadow: 0px 0px 0px 2px #0066FF;
    background-color: #FFFACD;
}
```

---

## 🔄 Qt ile Karşılaştırma

| Yön | Qt | İzekip UI |
|-----|----|-----------| 
| **Öğrenme Eğrisi** | Dik (MOC, Signal/Slot) | Eğimli (CSS + C++) |
| **Styling Dili** | QSS (özel syntax) | CSS (standart) |
| **Widget Sayısı** | 100+ (kompleks) | ~5 (minimal) |
| **Rendering** | Software + GPU hybrid | Pure GPU (D3D11/Vulkan) |
| **Cross-Platform** | ✅ Tamamlanmış | 🔄 Windows + Vulkan |
| **Metin Engine** | Qt's text engine | stb_truetype SDF |
| **Bağımlılık** | Ağır, kompleks setup | Hafif, CMake basit |
| **C++ Standard** | C++11+ | C++20 (modern) |
| **Hedef Kitle** | Geniş | Web dev'ler + C++ |

---

## 🌍 Platform Desteği

### Şu Anda Test Edilen

| Platform | Backend | Durum | Notlar |
|----------|---------|-------|--------|
| **Windows 10/11** | D3D11 | ✅ Tam | Native, MSVC 2026 |
| **Windows 10/11** | Vulkan | ✅ Tam | 1.4.363.0 test edildi |

### Planlanmış

| Platform | Backend | Durum | Notlar |
|----------|---------|-------|--------|
| **Linux** | Vulkan | 🔄 Ready | GLFW + Vulkan altyapısı var |
| **macOS** | Vulkan | 🔄 Ready | GLFW + Vulkan altyapısı var |

---

## 📜 Lisans & Katkı

### Lisans

**MIT License** - Eğitimsel ve ticari kullanıma izin verir

```
Bağımlılıklar:
├─ GLFW: zlib License
├─ Yoga: MIT License
├─ lexbor: Apache 2.0 License
├─ stb_truetype: Public Domain
└─ Inter Font: OFL (Open Font License)
```

### Katkı Alanları

```
✅ Açık: CSS özellik desteği (backdrop-filter, erf-blur)
✅ Açık: Yeni widget türleri (Checkbox, Select, Slider)
✅ Açık: Performans (batching, invalidation)
✅ Açık: Linux/macOS test ve doğrulama
✅ Açık: Dokümantasyon
❌ Kapalı: Signal/Slot (bilerek çıkarılmış)
```

---

## 🚀 İleride Planlar

### Kısa Vadeli (2-3 ay)

- [ ] Phase 7 tamamla (TextArea full featured)
- [ ] Linux Vulkan test
- [ ] Doku seçici (Checkbox) widget
- [ ] Text selection in TextArea

### Orta Vadeli (6 ay)

- [ ] macOS Vulkan support
- [ ] Slider widget
- [ ] CSS Grid layout başla
- [ ] Undo/redo system

### Uzun Vadeli (1+ yıl)

- [ ] Full Unicode support
- [ ] CSS custom properties (var())
- [ ] Animations (@keyframes)
- [ ] Web export (WASM via Emscripten)

---

## 📌 WIKIPEDIA'YA YAZARKEN DİKKAT ETMEN GEREKEN NOKTALAR

1. **Neutral Point of View (NPOV)**: Proje hakkında "en iyi" vs "kötü" deme
2. **Kaynakları Göster**: Teknik detaylar için README.md, ARCHITECTURE.md link ver
3. **Açık Kaynak Şartları**: MIT lisansını vurgula
4. **Karşılaştırma Dengeli**: Qt'le karşılaştırırken adil ol
5. **Türkçe Dil**: Wikipedia Türkçe standardlarına uy
6. **İç Linkler**: İlgili teknolojilere link ver (GLFW, Vulkan, etc.)
7. **Versiyon**: v1.0.0 ile başlıyor, semantic versioning takip ediyor

---

## 📚 Referanslar

- **README.md**: Quick start ve build
- **ARCHITECTURE.md**: Detailed API reference
- **CHANGELOG.md**: Versioning history
- **GitHub**: https://github.com/izekiptenknoloji-bit/
- **License**: MIT

---

**NOT**: Bu rehber 2026-10-11 tarihinde yazılmıştır. Proje değiştikçe güncellenmelidir.
