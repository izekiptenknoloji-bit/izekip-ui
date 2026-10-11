# İzekip UI - Wikipedia Makale Şablonu

## BAŞLIK
İzekip UI

## GİRİŞ KÖŞESİ (Infobox)
```
| Özellik | Bilgi |
|---------|-------|
| Yazılım türü | GUI framework (Arayüz Kütüphanesi) |
| Geliştirici | Furkan (izekiptenknoloji) |
| İlk sürüm | 2026 |
| Stabil sürüm | 1.0.0 |
| Yazılma dili | C++20 |
| İşletim sistemi | Windows, Linux, macOS (Vulkan) |
| Lisans | MIT |
| Web sitesi | GitHub |
```

---

## 1. AÇIKLAMA (Lede - İlk Paragraf)

**Yazılacak şekil:**
```
İzekip UI, 2026 yılında Furkan tarafından geliştirilmeye başlanan açık kaynak
C++ grafik arayüz (GUI) kütüphanesidir. Qt kütüphanesine daha basit ve web
geliştiricileri için daha uyumlu bir alternatif sunmayı hedeflemektedir. 
Gerçek CSS stil desteği, GPU tabanlı hızlı rendering (DirectX 11 ve Vulkan) 
ve Facebook'un Yoga flexbox motorunu kullanarak modern, responsive arayüzler 
oluşturmaya imkan verir.
```

---

## 2. TARİH & GENEL BAKIŞ

### 2.1 Neden Yapıldı? (Motivasyon)
```
Qt kütüphanesinin öğrenilmesi zor olması (MOC pre-compiler, Signal/Slot sistemi,
karmaşık CMake kurulumu) başlıca neden. Web geliştiricilerin CSS dilini bildikleri
varsayıldığında, CSS ile UI stilleri tanımlamak daha doğal ve öğrenmesi kolay olur.

Ayrıca modern GPU'ların gücünden tam olarak faydalanmak (Pure GPU rendering) ve
cross-platform desteği (Windows, Linux, macOS) sağlamak amaçlarıydı.
```

### 2.2 Temel Vizyonu
```
Bir single rendering layer (tek render katmanı) oluşturmak ki Windows'ta (D3D11)
ve Linux/macOS'ta (Vulkan) pixel-perfect (piksel-mükemmel) aynı görüntüyü verebilsin.
CSS'i web'den gelen tanıdık bir dil olarak kullanarak yeni başlayanlar için
öğrenme eğrisini düşürmek.
```

---

## 3. TEMEL ÖZELLİKLER

### 3.1 CSS Styling
```
İzekip UI'ın en önemli özelliği gerçek CSS (Cascading Style Sheets) dilini
kullanmasıdır. Bu, web geliştiricilerin CSS'i ve web standartlarını
C++ GUI geliştirmeye aktarmasını sağlar.

Desteklenen CSS:
- Type selectors (Button { }, Panel { })
- Class selectors (.primary, .large)
- ID selectors (#root, #main-content)
- Descendant combinators (Panel > Button)
- Pseudo-classes (:hover, :active, :focus)
- Color formats (hex #FF6B6B, rgb, rgba, named colors)
- Sizing units (px, %)
- Flexbox properties (flex-direction, justify-content, align-items)
- Visual effects (border-radius, box-shadow, linear-gradient, opacity)
- Animations (transition property with cubic-bezier easing)
```

### 3.2 GPU Rendering
```
İzekip UI sadece GPU (Graphics Processing Unit - grafik kartı) kullanarak
rendering yapar. İki farklı backend desteği:

DirectX 11:
- Windows native GPU API
- MSVC 2026 derleyicisiyle derlenir
- Runtime'da D3DCompile ile shader derlemesi

Vulkan:
- Khronos standardı, cross-platform
- Windows, Linux, macOS'ta çalışır
- Build-time'da dxc.exe ile SPIR-V derlemesi
- Her iki backend pixel-identical output üretir
```

### 3.3 Flexbox Layout
```
Facebook'un geliştirdiği Yoga layout motorunu kullanan responsive design desteği.
Web'deki flexbox'ı C++ arayüzlerine getirerek tanıdık ve güçlü layout sistemi sağlar.
```

### 3.4 Metin Rendering (SDF)
```
stb_truetype kütüphanesi ile TrueType fontlarını SDF (Signed Distance Field)
olarak 512x512 texture'a bak. Zoom'da bile temiz, keskin yazı görüntüsü.
Türkçe karakter desteği (ş, ğ, ı, İ, vb.) yapılmıştır.
```

---

## 4. TEKNİK YAPI

### 4.1 Tech Stack Tablosu
```
| Bileşen | Seçim | Açıklama |
|---------|-------|----------|
| Dil | C++20 | Modern features, performans |
| Pencere | GLFW 3.x | Cross-platform windowing |
| CSS Parser | lexbor | W3C standartları uyumlu |
| Layout | Yoga | Facebook flexbox motor |
| GPU D3D11 | DirectX 11 | Windows native |
| GPU Vulkan | Vulkan 1.4 | Cross-platform |
| Metin | stb_truetype | Font rasterization |
| Build | CMake 3.21+ | Standard C++ build system |
```

### 4.2 Proje Mimarisi
```
Dosya yapısı:

include/       - Public API headers
├── Core/      - Widget, Animations, Layout
├── CSS/       - Parser, StyleApplier, StyleSheet
├── Renderer/  - IRenderer abstract interface
├── Platform/  - Window (GLFW wrapper)
└── Widgets/   - Panel, Button, Label, TextArea

src/           - Implementation
├── Renderer/
│   ├── D3D11/     - DirectX backend
│   ├── Vulkan/    - Vulkan backend
│   └── Shaders/   - RectShader.hlsl (shared)

third_party/   - Vendored dependencies
├── glfw/
├── yoga/
├── lexbor/
├── stb/
└── fonts/

examples/      - Demo applications
tests/         - Unit tests
cmake/         - Build configuration
```

### 4.3 Render Pipeline
```
CSS File + Widget Tree (C++)
    ↓
lexbor CSS Parser
    ↓
LexborBridge (Widget → DOM mapping)
    ↓
StyleApplier (CSS cascade, specificity)
    ↓
Yoga Layout (Flexbox calculations)
    ↓
RectDrawData (CPU-side math)
    ↓
RectShader.hlsl
    ├─ D3D11: Runtime D3DCompile
    └─ Vulkan: Build-time dxc SPIR-V
    ↓
GPU Output (Identical both backends)
```

---

## 5. GELİŞTİRME AŞAMALARI (Phases)

### Phase 1: MVP (Minimum Viable Product)
```
Tarih: 2026 başlangıcı

İçerik:
- GLFW pencere oluşturma
- DirectX 11 device ve swapchain initialization
- Hand-rolled CSS parser (type/.class/#id only)
- Yoga layout engine integration
- Flat-color rect rendering

Sonuç: examples/01_basic_panel_button örneği çalışan ilk sürüm
```

### Phase 2: Interactive UI
```
Tarih: 2026 erken aşama

İçerik:
- GLFW mouse/cursor callbacks
- Widget::HitTest (hit detection)
- Hover state management
- :hover ve :active CSS pseudo-classes
- CSS transition property desteği
- Cubic-bezier easing engine (Newton-Raphson)
- Smooth animasyonlar (color, size, margin)

Sonuç: Animated button örneği, interactive UI
```

### Phase 3: Visual Effects
```
Tarih: 2026 orta aşama

İçerik:
- border-radius (SDF shader)
- box-shadow (gölge efekti)
- linear-gradient (geçişli renkler)
- Premultiplied alpha blending
- Inigo Quilez SDF algorithms

Karar: Bu özellikler henüz animate edilmiyor (snap-only)

Sonuç: Modern, güzel UI görüntüsü
```

### Phase 4: Vulkan Backend
```
Tarih: 2026 orta-sonu

İçerik:
- İkinci IRenderer implementasyonu
- SPIR-V shader compilation (build-time dxc)
- Shared RectShader.hlsl
- Vulkan surface creation (GLFW bridge)
- Swapchain management ve recreation
- Semaphore/fence synchronization
- Validation layer testing

Donanım: Intel UHD Graphics, Vulkan 1.4

Sonuç: D3D11 ve Vulkan pixel-identical output
```

### Phase 5: Standards-Compliant CSS
```
Tarih: 2026 sonu

İçerik:
- Hand-rolled parser'ı lexbor'a geçiş
- Descendant combinators (> operator)
- Multi-value CSS shorthands (margin: 10px 20px 30px 40px)
- flex-direction, justify-content, align-items Yoga'ya wiring
- Proper CSS cascade ve specificity

Sonuç: W3C CSS standartlarına uyumlu parsing
```

### Phase 6: Text Rendering (SDF)
```
Tarih: 2026-10 sonu

İçerik:
- stb_truetype glyph rasterization
- 512x512 SDF atlas texture
- RectShader.hlsl'e text mode flag
- Widgets::Label (single-line text)
- Font sizing via Yoga's YGNodeSetMeasureFunc
- Latin-1 + Turkish extended characters
- D3D11 + Vulkan texture/sampler support

Sonuç: Türkçe karakterlerle metin gösterim
```

### Phase 7: TextArea & Keyboard (Devam Ediyor)
```
Tarih: 2026-11 ve sonrası

İçerik:
- Widgets::TextArea (multi-line editable)
- TextBuffer data structure
- Keyboard event handling
- Platform::Window callbacks
- :focus pseudo-class
- Cursor positioning

Scope kararı: Text selection, clipboard, undo-redo henüz yok

Sonuç: examples/03_notepad örneği
```

### Phase 8+ (Planlanmış, Başlanmamış)
```
Hedefler:
- Checkbox, Select, Slider widgets
- Text selection + clipboard + undo/redo
- Full Unicode support
- CSS Grid layout
- Absolute positioning + z-index
- font-weight, font-family CSS properties
- Linux/macOS native testing
```

---

## 6. KOD ÖRNEĞI

### 6.1 Basit Buton (C++ + CSS)

**C++ Kodu:**
```cpp
auto root = std::make_shared<Widgets::Panel>();
root->SetId("root");
root->AddClass("container");

auto button = std::make_shared<Widgets::Button>();
button->AddClass("primary");
root->AddChild(button);

Css::StyleSheet sheet = Css::StyleSheet::LoadFromFile("style.css");
Css::LexborBridge bridge;
bridge.Build(*root);
Css::StyleApplier::Apply(bridge, sheet, *root);
```

**CSS Kodu:**
```css
Panel#root.container {
    background-color: #F5F5F5;
    width: 800px;
    height: 600px;
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
```

**Ne yapıyor:**
- Kök panel (background + centered layout)
- Gradient buton (sol mavi → sağ açık mavi)
- Hover'da opaklik azalır ve gölge çıkar
- 300ms smooth transition

---

## 7. Qt İLE KARŞILAŞTIRMA

```
| Özellik | Qt | İzekip UI |
|---------|----|-----------| 
| Öğrenme Eğrisi | Dik | Eğimli |
| Styling | QSS (custom) | CSS (standard) |
| Widget Sayısı | 100+ | ~5 |
| Rendering | Software+GPU | Pure GPU |
| Cross-Platform | ✅ Tam | 🔄 Windows+Vulkan |
| Metin Engine | Qt's engine | stb_truetype SDF |
| C++ Standard | C++11+ | C++20 |
| Setup | Ağır | Hafif |
| Hedef Kitle | Geniş | Web dev'ler |
```

**Temel Fark:**
Qt, geleneksel "tüm şeyi C++ kodunda yapmak" yaklaşımı izlerken;
İzekip UI, "C++ ile logic, CSS ile style" web paradigmasını kullanır.

---

## 8. PLATFORM DESTEĞİ

### Şu Anda Test Edilen
```
Windows 10/11:
- D3D11 Backend: ✅ Tam desteklenen
- Vulkan Backend: ✅ Tam desteklenen (v1.4.363.0 test edildi)

Build Tools:
- Visual Studio 2026 (Community)
- CMake 4.3.1
- Windows SDK 10.0.26100.0
- Vulkan SDK 1.4.363.0
```

### Planlanmış
```
Linux:
- Vulkan backend (GLFW + Vulkan altyapısı hazır)
- Status: 🔄 Ready, test bekleniyor

macOS:
- Vulkan backend
- Status: 🔄 Ready, test bekleniyor
```

---

## 9. LİSANS & KATKI

### Lisans
```
MIT License - Eğitimsel ve ticari kullanıma izin verir

Bağımlılık Lisansları:
- GLFW: zlib License
- Yoga: MIT License
- lexbor: Apache 2.0 License
- stb_truetype: Public Domain
- Inter Font: OFL (Open Font License)
```

### Katkı Alanları
```
Açık:
- CSS özellik desteği (backdrop-filter enhancements)
- Yeni widget türleri (Checkbox, Select, Slider)
- Performans optimizasyonu (batching, invalidation)
- Linux/macOS testing
- Dokümantasyon

Intentionally Out of Scope:
- Signal/Slot sistemi (bilerek çıkarılmış)
- 100+ widget (minimal approach tercih)
- Qt compatibility mode (Qt'den ayrı proje)
```

---

## 10. DESTEKLENEN CSS ÖZELLİKLERİ

### Layout & Sizing
```
- width (px, %)
- height (px, %)
- margin (1-4 value shorthand)
- padding (1-4 value shorthand)
- flex-direction (row, column, row-reverse, column-reverse)
- justify-content (flex-start, flex-end, center, space-between, space-around)
- align-items (flex-start, flex-end, center, stretch)
```

### Colors & Fills
```
- background-color (hex, rgb, rgba, named colors)
- background: linear-gradient(direction, color1, color2)
- color (metin rengi)
- border-color
- opacity (0.0 - 1.0)
```

### Visual Effects
```
- border-radius (SDF-based)
- box-shadow (multi-shadow support)
- backdrop-filter (blur, brightness, contrast, saturate, grayscale)
```

### Animations
```
- transition: property duration easing-function;
- Easing: linear, ease-in, ease-out, ease-in-out, cubic-bezier(x1,y1,x2,y2)
- Animatable: background-color, width, height, margin, opacity
```

### Selectors
```
- Type: Button { }
- Class: .primary { }
- ID: #root { }
- Descendant: Panel > Button { }
- Pseudo-classes: :hover, :active, :focus
```

---

## 11. İNŞA (BUILD) & KURULUM

### D3D11 Build (Windows)
```bash
mkdir build
cd build
cmake .. -G "Visual Studio 18 2026" -DIZEKIP_RENDER_BACKEND=D3D11
cmake --build . --config Release

# Çalıştır
.\Release\IzekipUIExample01.exe
```

### Vulkan Build
```bash
# Vulkan SDK'yı ayarla (gerekirse)
$env:VULKAN_SDK = "C:\VulkanSDK\1.4.363.0"

mkdir build-vulkan
cd build-vulkan
cmake .. -DIZEKIP_RENDER_BACKEND=VULKAN
cmake --build . --config Release

.\Release\IzekipUIExample01.exe
```

### Örnek Uygulamalar
```
examples/01_basic_panel_button/
- Temel özellikler: selectors, layout, effects, animations

examples/02_text_document/
- Label widget, CSS dokümantasyon, backdrop-filter

examples/03_notepad/
- TextArea, keyboard events, :focus pseudo-class, Turkish text
```

---

## 12. KAYNAKLAR & REFERANSLAR

```
GitHub: https://github.com/izekiptenknoloji-bit/

Bağlantılı Projeler:
- GLFW: https://github.com/glfw/glfw
- Yoga: https://github.com/facebook/yoga
- lexbor: https://github.com/lexbor/lexbor
- stb: https://github.com/nothings/stb

Standartlar:
- W3C CSS Specification
- Flexbox Layout Module
- Vulkan Specification
- DirectX Documentation
```

---

## 13. HAKKINDAKİ

### Geliştirici
```
Furkan (izekiptenknoloji)
Email: izekiptenknoloji@gmail.com
GitHub: https://github.com/izekiptenknoloji-bit
```

### Proje Bilgileri
```
İlk Sürüm: 2026
Stabil Sürüm: 1.0.0
Sürümleme: Semantic Versioning (MAJOR.MINOR.PATCH)
Lisans: MIT
```

### İlgili Sayfalar
```
- Qt (Operating system-independent GUI framework)
- GLFW (Graphics Library Framework)
- Flexbox (CSS Layout)
- Vulkan (Graphics API)
- DirectX (Windows Graphics API)
- GPU (Graphics Processing Unit)
```

---

## 14. İLETİŞİM KUTULARI

### Bilgi Kutusu (Template)
```
| Bilgi | Değer |
|-------|-------|
| Tür | Open source GUI framework |
| Geliştirici | Furkan |
| İlk sürüm | 2026 |
| Stabil sürüm | 1.0.0 |
| Yazılım dili | C++20 |
| İşletim sistemi | Windows, Linux, macOS |
| Lisans | MIT |
| Depo | GitHub |
```

### Uyarı Kutusu
```
Bu proje 2026 yılında aktif geliştirme aşamasındadır.
API'nin değişmesi mümkündür. Üretim ortamında kullanırken dikkat edin.
```

### Başlıca Özellikler Kutusu
```
✅ Gerçek CSS Styling
✅ GPU Rendering (D3D11 + Vulkan)
✅ Flexbox Layout
✅ Smooth Animasyonlar
✅ SDF Metin Rendering
✅ Türkçe Karakter Desteği
```

---

## NOTLAR (Editörlere)

1. **Neutral Point of View**: Proje hakkında "en iyi" vs "kötü" yazmasın
2. **Kaynaklar**: Teknik detaylar için GitHub repo ve README.md'ye link ver
3. **Dış Linkler**: Wikipedia dış link politikasına uygun olacak şekilde sınırla
4. **İç Linkler**: İlgili teknolojilere (Qt, Vulkan, CSS, etc.) link ver
5. **Güncelleme**: Proje geliştikçe aşamaları, özelikleri ve platform desteğini güncelle
6. **Türkçe Dilbilgisi**: Wikipedia Türkçe yazım kurallarına uy
7. **Başlıklandırma**: H2 (==) ana başlıklar, H3 (===) alt başlıklar

---

**Bu şablonu kopyala, Wikipedia'ya yapıştır ve ellerinle düzenle!**

İhtiyacın olan bölüm var mı?
