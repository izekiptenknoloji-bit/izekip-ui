# İzekip UI — Yapay Zeka Asistanları İçin Öğretim Metni

Bu dosya, İzekip UI SDK'sını kullanan bir projede çalışırken Claude Code (ya da başka bir
yapay zeka asistanı) için kısa bir rehberdir. Kurulumda "Claude Code için öğretim dosyasını
ekle" seçeneğini işaretlediyseniz bu dosya `share/doc/IzekipUI/AI_GUIDE.md` altına kopyalanır.
Asistanınıza projeyi anlatırken bu dosyanın yolunu verin.

## Ne bu kütüphane?

İzekip UI, C++20 ile yazılmış bir GUI kütüphanesidir. Arayüz **gerçek CSS** ile stillenir:
sınıf (`.class`), tip (`Button`), kimlik (`#id`) seçicileri, `:hover`/`:active`/`:focus`
durumları ve flexbox düzeni desteklenir. Düzen Yoga, CSS ayrıştırması lexbor ile yapılır,
çizim D3D11 (Windows) veya Vulkan ile GPU üzerinde gerçekleşir.

Temel fikir: **görsel özellikler CSS'te, mantık C++'ta.** Yeni bir görünüm için önce CSS'e
bakın, C++ tarafında `Set...` çağrısı yapmak çoğu zaman gereksizdir.

## Minimum çalışan program

```cpp
#include "Platform/Window.h"
#include "Renderer/IRenderer.h"
#include "Widgets/Panel.h"
#include "Widgets/Label.h"
#include "CSS/StyleSheet.h"
#include "CSS/StyleApplier.h"
#include "CSS/LexborBridge.h"

using namespace IzekipUI;

int main() {
    Platform::Window window({.title = "Örnek", .width = 800, .height = 600});

    auto renderer = Render::CreateRenderer(Render::RendererBackend::D3D11);
    Render::RendererCreateInfo info{
        .nativeWindowHandle = window.GetNativeHandle(),
        .glfwWindow = window.GetGLFWHandle(),
        .initialWidth = 800,
        .initialHeight = 600,
    };
    renderer->Initialize(info);

    auto root = std::make_shared<Widgets::Panel>();
    root->SetId("root");
    auto title = std::make_shared<Widgets::Label>();
    title->SetText("Merhaba, İzekip UI");
    title->AddClass("title");
    root->AddChild(title);

    // Asset'ler (fontlar, shader, css) çalışma dizinine göre yüklenir: exe'nin yanına koyun.
    Css::StyleSheet sheet = Css::StyleSheet::LoadFromFile("style.css");
    Css::LexborBridge bridge;
    bridge.Build(*root);                       // ağaç ve id'ler burada kilitlenir
    Css::StyleApplier::Apply(bridge, sheet, *root);
    root->CalculateLayout(800.0f, 600.0f);

    while (!window.ShouldClose()) {
        window.PollEvents();
        renderer->BeginFrame();
        root->OnPaint(*renderer);
        renderer->EndFrame();
        renderer->Present();
    }
    renderer->Shutdown();
}
```

Sınıf adları (`Widgets::Panel`, `Widgets::Label`, `Widgets::Button`, `Widgets::TextArea`,
`Widgets::Checkbox`, `Widgets::RadioButton`, `Widgets::RadioGroup`, `Widgets::ProgressBar`)
`include/Widgets/` altındaki başlıklarda tanımlıdır. Bir kontrolün davranışından emin değilseniz
ilgili `.h` dosyasının yorumlarını okuyun; `include/` dizini SDK'nın içinde gelir.

## Widget'lar ve kalıplar

- **Button**: İçine bir `Label` eklenir (`button->AddChild(label)`), tıklama için
  `SetOnClick([]{ ... })`. Buton görünümü CSS'te `.btn` gibi bir sınıfla verilir.
- **Label**: `SetText` UTF-8 metin alır; Türkçe karakterler (ş ğ ı ö ü ç İ) doğru çizilir.
- **TextArea**: Tek satır/çok satır düzenlenebilir metin. Kutu görünümü için kenarlık ve
  arka planı bir sarmalayıcı `Panel`'e verin; TextArea'nın kendi padding'i glifleri
  kaydırmaz.
- **Checkbox / RadioButton**: Durum `checked` sınıfıyla temsil edilir; görünüm CSS'ten gelir.
  Birbirini dışlayan seçimler için `RadioGroup` kullanın: `Add(button)`, `Select(index)`,
  `SetOnChanged([](int i){ ... })`.
- **ProgressBar**: `SetValue(0..1)`. İç dolgu `.progress-fill` çocuğudur; dolgunun
  genişliğini CSS'ten `width` ile vermeyin, kütüphane yüzdeyi kendisi yazar.

## Durum sınıfları ve CSS kuralları (sık yapılan hatalar)

1. **Kademeli (additive) kaskad.** Bir durum sınıfı (`.hidden`, `.checked`, `:hover`) bir
   özelliği değiştiriyorsa, o özelliğin **temel (base) değerini de** aynı kuralda tanımlayın.
   Aksi halde sınıf kaldırıldığında eski değer kalır.
2. **Gizleme.** `display: none` olan bir çocuğu ebeveyn çizmez ve tıklamayı kabul etmez.
   Sayfa geçişleri için `.hidden { display: none; }` kullanın.
3. **Sınıf değişikliği.** Bir widget'ın sınıflarını çalışma anında değiştirdiyseniz stilin
   yeniden uygulanması gerekir: `StyleApplier::Apply` çağrısı ya da
   `InteractionController::RequestRestyle()`. Tıklama içinde yapılan değişiklik zaten
   bir sonraki güncellemede uygulanır.
4. **Kare düzeni.** Yoga düzeni yalnızca `width`, `height`, `margin`, `padding`,
   `flex-direction`, `justify-content`, `align-items`, `display` değerlerinden hesaplanır.
   `scale` ve `rotate` düzeni etkilemez.
5. **Dosya yolları.** Fontlar ve shader çalışma dizinine göre aranır. Program
   Uygulamalar'dan ya da farklı bir dizinden açılıyorsa asset'leri exe'nin yanında tutun.

## Desteklenen CSS

Seçiciler: tip, `.sınıf`, `#id`, boşluk (torun) birleştiricisi, `:hover`, `:active`, `:focus`.

| Özellik | Notlar |
|---|---|
| `width`, `height`, `min-*`, `max-*` | px, %; `height` için `vh` |
| `margin`, `padding` | 1–4 değerli kısaltma, px |
| `flex-direction`, `justify-content`, `align-items`, `display` | `display` yalnızca `flex` ve `none` |
| `background-color`, `background` | `background` yalnızca doğrusal gradyan |
| `color`, `font-size`, `font-family` | `font-family`: `sans-serif`, `serif`, `system-ui`, `monospace` |
| `font-weight` | 600 ve üzeri sentetik kalınlaştırma (ayrı kalın font dosyası yok) |
| `border`, `border-color`, `border-width`, `border-radius` | |
| `box-shadow` | çoklu gölge |
| `backdrop-filter` | `blur`, `brightness`, `contrast`, `saturate`, `grayscale`, `hue-rotate` |
| `opacity` | animasyonlu |
| `transform` | yalnızca `translate` / `translateX` / `translateY` |
| `scale` | sayı (> 0), widget merkezine göre |
| `rotate` | açı (`deg`, `rad`, `turn`, `grad`), widget merkezine göre |
| `cursor` | `default`, `pointer`, `text`, `crosshair`, `not-allowed`; ebeveynden miras alınır |
| `transition` | `background-color`, `width`, `height`, `margin`, `opacity`, `transform`, `all` |
| `animation` / `@keyframes` | tek adlandırılmış animasyon; `opacity` ve translate `transform` |

Desteklenmeyen özellikler sessizce yok sayılır. Bir özelliğin çalışmadığını düşünüyorsanız
önce bu tabloya bakın.

## Sınırlamalar

- `scale` ve `rotate` transition ile animasyonlanmaz; anlık değişir.
- `backdrop-filter` ve gölge bulanıklığı dönüşümün ölçeğini almaz.
- Image widget'ı henüz görüntü dosyası çizmez (yer tutucu).
- Yalnızca Windows (D3D11 veya Vulkan) hedeflenir; Linux/macOS planlanıyor.

## Yapay zeka asistanına notlar

- Bu kütüphanede Python ya da başka bir script dili kullanılmaz; derleme CMake ile yapılır.
- Kullanıcı arayüzü Türkçe metinlerle yazılıyorsa dosyaları UTF-8 kaydedin.
- Mevcut bir örneği değiştirmeden önce `tests/` altındaki ilgili testlere bakın; CSS ayrıştırma
  ve etkileşim davranışları oradaki testlerle doğrulanır (`ctest -C Release`).
- Gerçek bir API değişikliği yapmadan önce `COMPATIBILITY.md` dosyasını okuyun: `include/`
  altındaki başlıklar kararlılık katmanlarına ayrılmıştır (Tier-1/Tier-2).
