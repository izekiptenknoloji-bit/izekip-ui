// ===================================
// İZEKİP UI - JAVASCRIPT INTERACTIVE
// ===================================

// ===== SMOOTH SCROLLING & NAVIGATION =====
document.addEventListener('DOMContentLoaded', function() {
    console.log('✨ İzekip UI Web Sayfası Yüklendi');

    // Navigation links smooth scroll
    const navLinks = document.querySelectorAll('.nav-link');
    navLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            e.preventDefault();
            const targetId = this.getAttribute('href');
            if (targetId.startsWith('#')) {
                const targetElement = document.querySelector(targetId);
                if (targetElement) {
                    targetElement.scrollIntoView({
                        behavior: 'smooth',
                        block: 'start'
                    });
                }
            }
        });
    });

    // Initialize tabs
    initializeTabs();
    initializeBuildTabs();
    initializeFeatureCards();
});

// ===== TAB SYSTEM =====
function initializeTabs() {
    const tabButtons = document.querySelectorAll('.tab-btn');
    const tabContents = document.querySelectorAll('.tab-content');

    tabButtons.forEach(button => {
        button.addEventListener('click', function() {
            // Tab butonunu aktif yap
            tabButtons.forEach(btn => btn.classList.remove('active'));
            this.classList.add('active');

            // Tab içeriğini göster
            tabContents.forEach(content => content.classList.remove('active'));
            const tabId = this.getAttribute('data-tab');
            const targetTab = document.getElementById(tabId);
            if (targetTab) {
                targetTab.classList.add('active');
            }
        });
    });
}

// ===== BUILD TABS =====
function initializeBuildTabs() {
    const buildTabButtons = document.querySelectorAll('.build-tab-btn');
    const buildTabContents = document.querySelectorAll('.build-tab-content');

    buildTabButtons.forEach(button => {
        button.addEventListener('click', function() {
            // Button'u aktif yap
            buildTabButtons.forEach(btn => btn.classList.remove('active'));
            this.classList.add('active');

            // Content'i göster
            buildTabContents.forEach(content => content.classList.remove('active'));
            const tabId = this.getAttribute('data-build-tab');
            const targetTab = document.getElementById(tabId);
            if (targetTab) {
                targetTab.classList.add('active');
            }
        });
    });
}

// ===== FEATURE CARDS INTERACTIVITY =====
function initializeFeatureCards() {
    const featureCards = document.querySelectorAll('.feature-card');

    featureCards.forEach(card => {
        card.addEventListener('mouseenter', function() {
            // Feature card hover efekti
            this.style.transform = 'translateY(-8px)';
        });

        card.addEventListener('mouseleave', function() {
            this.style.transform = 'translateY(0)';
        });

        // Click efekti
        card.addEventListener('click', function() {
            const feature = this.getAttribute('data-feature');
            console.log(`📌 Özellik seçildi: ${feature}`);
        });
    });
}

// ===== SYNTAX HIGHLIGHTING SIMULATION =====
function highlightCode() {
    const codeBlocks = document.querySelectorAll('pre code');

    codeBlocks.forEach(block => {
        const lang = block.className.replace('language-', '');

        // Basit syntax highlighting
        let html = block.innerHTML;

        if (lang === 'cpp') {
            html = highlightCpp(html);
        } else if (lang === 'css') {
            html = highlightCss(html);
        } else if (lang === 'bash') {
            html = highlightBash(html);
        }

        block.innerHTML = html;
    });
}

function highlightCpp(code) {
    // C++ keywords vurgulama
    const keywords = ['auto', 'void', 'int', 'float', 'std', 'shared_ptr', 'make_shared',
                      'class', 'struct', 'template', 'if', 'else', 'for', 'while'];

    let highlighted = code;

    // Keywords
    keywords.forEach(keyword => {
        const regex = new RegExp(`\\b${keyword}\\b`, 'g');
        highlighted = highlighted.replace(regex, `<span class="keyword">${keyword}</span>`);
    });

    // String literals
    highlighted = highlighted.replace(/"([^"]*)"/g, '<span class="string">"$1"</span>');

    // Comments
    highlighted = highlighted.replace(/\/\/.*$/gm, '<span class="comment">$&</span>');

    return highlighted;
}

function highlightCss(code) {
    let highlighted = code;

    // CSS properties
    highlighted = highlighted.replace(/([a-z-]+):/g, '<span class="property">$1</span>:');

    // Values (colors, sizes)
    highlighted = highlighted.replace(/(#[0-9A-Fa-f]+|[0-9]+px|[0-9]+%)/g, '<span class="value">$1</span>');

    // Selectors
    highlighted = highlighted.replace(/^([^{]+)\{/gm, '<span class="selector">$1</span>{');

    return highlighted;
}

function highlightBash(code) {
    let highlighted = code;

    // Commands
    highlighted = highlighted.replace(/^(mkdir|cd|cmake|git|echo)/gm, '<span class="command">$1</span>');

    // Flags
    highlighted = highlighted.replace(/(-[a-zA-Z0-9]+)/g, '<span class="flag">$1</span>');

    // Paths
    highlighted = highlighted.replace(/(\/)([a-zA-Z0-9._-]+)+/g, '<span class="path">$&</span>');

    return highlighted;
}

// ===== SCROLL ANIMATIONS =====
const observerOptions = {
    threshold: 0.1,
    rootMargin: '0px 0px -50px 0px'
};

const observer = new IntersectionObserver(function(entries) {
    entries.forEach(entry => {
        if (entry.isIntersecting) {
            entry.target.style.animation = 'fadeIn 0.8s ease-in-out forwards';
            observer.unobserve(entry.target);
        }
    });
}, observerOptions);

// Observer'ı başlat
document.querySelectorAll('.feature-card, .phase-item, .css-category').forEach(el => {
    observer.observe(el);
});

// ===== COPY CODE BUTTON =====
function addCopyButtons() {
    const preBlocks = document.querySelectorAll('pre');

    preBlocks.forEach(pre => {
        const copyBtn = document.createElement('button');
        copyBtn.innerHTML = '📋 Kopyala';
        copyBtn.className = 'copy-btn';
        copyBtn.style.cssText = `
            position: absolute;
            top: 8px;
            right: 8px;
            padding: 6px 12px;
            background-color: #667EEA;
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 12px;
            z-index: 10;
        `;

        pre.style.position = 'relative';
        pre.appendChild(copyBtn);

        copyBtn.addEventListener('click', function() {
            const code = pre.querySelector('code').innerText;
            navigator.clipboard.writeText(code).then(() => {
                copyBtn.innerHTML = '✓ Kopyalandı!';
                setTimeout(() => {
                    copyBtn.innerHTML = '📋 Kopyala';
                }, 2000);
            }).catch(err => {
                console.error('Kopyalama başarısız:', err);
            });
        });
    });
}

// Copy butonlarını ekle
document.addEventListener('DOMContentLoaded', addCopyButtons);

// ===== SCROLL TO TOP BUTTON =====
function createScrollToTopButton() {
    const scrollBtn = document.createElement('button');
    scrollBtn.innerHTML = '↑';
    scrollBtn.className = 'scroll-to-top';
    scrollBtn.style.cssText = `
        position: fixed;
        bottom: 30px;
        right: 30px;
        width: 50px;
        height: 50px;
        background: linear-gradient(135deg, #667EEA, #764BA2);
        color: white;
        border: none;
        border-radius: 50%;
        cursor: pointer;
        font-size: 24px;
        display: none;
        z-index: 999;
        transition: all 0.3s ease-in-out;
        box-shadow: 0 4px 12px rgba(102, 126, 234, 0.4);
    `;

    document.body.appendChild(scrollBtn);

    window.addEventListener('scroll', () => {
        if (window.scrollY > 300) {
            scrollBtn.style.display = 'block';
        } else {
            scrollBtn.style.display = 'none';
        }
    });

    scrollBtn.addEventListener('click', () => {
        window.scrollTo({
            top: 0,
            behavior: 'smooth'
        });
    });

    scrollBtn.addEventListener('mouseover', () => {
        scrollBtn.style.transform = 'scale(1.1)';
    });

    scrollBtn.addEventListener('mouseout', () => {
        scrollBtn.style.transform = 'scale(1)';
    });
}

document.addEventListener('DOMContentLoaded', createScrollToTopButton);

// ===== FEATURE COUNTER ANIMATION =====
function animateCounters() {
    const counters = document.querySelectorAll('.counter');

    counters.forEach(counter => {
        const target = parseInt(counter.getAttribute('data-target'));
        const increment = target / 100;
        let current = 0;

        const updateCounter = () => {
            if (current < target) {
                current += increment;
                counter.textContent = Math.floor(current);
                requestAnimationFrame(updateCounter);
            } else {
                counter.textContent = target;
            }
        };

        updateCounter();
    });
}

// ===== PHASE FILTER =====
function setupPhaseFilter() {
    const phases = document.querySelectorAll('.phase-item');

    // Filter butonları oluştur
    const filterContainer = document.createElement('div');
    filterContainer.style.cssText = `
        display: flex;
        gap: 12px;
        justify-content: center;
        margin-bottom: 40px;
        flex-wrap: wrap;
    `;

    const filters = [
        { label: 'Tümü', class: 'all' },
        { label: 'Tamamlandı', class: 'completed' },
        { label: 'Devam Ediyor', class: 'in-progress' },
        { label: 'Planlanmış', class: 'planned' }
    ];

    filters.forEach(filter => {
        const btn = document.createElement('button');
        btn.innerHTML = filter.label;
        btn.style.cssText = `
            padding: 10px 20px;
            background-color: #F0F0F0;
            border: 2px solid transparent;
            border-radius: 6px;
            cursor: pointer;
            font-weight: 600;
            transition: all 0.3s ease;
        `;

        if (filter.class === 'all') {
            btn.style.borderColor = '#667EEA';
            btn.style.backgroundColor = '#667EEA';
            btn.style.color = 'white';
        }

        btn.addEventListener('click', () => {
            // Tüm butonların stilini sıfırla
            filterContainer.querySelectorAll('button').forEach(b => {
                b.style.borderColor = 'transparent';
                b.style.backgroundColor = '#F0F0F0';
                b.style.color = 'inherit';
            });

            // Seçili butonu vurgula
            btn.style.borderColor = '#667EEA';
            btn.style.backgroundColor = '#667EEA';
            btn.style.color = 'white';

            // Phases'i filtrele
            phases.forEach(phase => {
                if (filter.class === 'all') {
                    phase.style.display = 'block';
                } else {
                    if (phase.classList.contains(filter.class)) {
                        phase.style.display = 'block';
                    } else {
                        phase.style.display = 'none';
                    }
                }
            });
        });

        filterContainer.appendChild(btn);
    });

    const phasesSection = document.querySelector('.phases-timeline');
    if (phasesSection && phasesSection.parentElement) {
        phasesSection.parentElement.insertBefore(filterContainer, phasesSection);
    }
}

document.addEventListener('DOMContentLoaded', setupPhaseFilter);

// ===== DARK MODE TOGGLE (Optional) =====
function setupDarkMode() {
    // Local storage'dan dark mode durumunu al
    const isDarkMode = localStorage.getItem('darkMode') === 'true';

    if (isDarkMode) {
        document.body.classList.add('dark-mode');
    }

    // Dark mode switch (HTML'ye eklenmek gerekebilir)
    const darkModeSwitch = document.querySelector('.dark-mode-switch');
    if (darkModeSwitch) {
        darkModeSwitch.addEventListener('click', () => {
            document.body.classList.toggle('dark-mode');
            localStorage.setItem('darkMode', document.body.classList.contains('dark-mode'));
        });
    }
}

// Dark mode CSS'i dinamik olarak ekle
function injectDarkModeStyles() {
    const style = document.createElement('style');
    style.innerHTML = `
        body.dark-mode {
            background-color: #1E1E1E;
            color: #E0E0E0;
        }

        body.dark-mode .feature-card,
        body.dark-mode .tech-stack,
        body.dark-mode .pipeline-diagram,
        body.dark-mode .file-structure,
        body.dark-mode .comparison-table,
        body.dark-mode .css-category {
            background-color: #2D2D2D;
            color: #E0E0E0;
        }

        body.dark-mode .code-panel {
            background-color: #1E1E1E;
        }

        body.dark-mode code {
            background-color: #2D2D2D;
            color: #E0E0E0;
        }
    `;
    document.head.appendChild(style);
}

document.addEventListener('DOMContentLoaded', () => {
    injectDarkModeStyles();
    setupDarkMode();
});

// ===== LOGGER =====
console.log('%c🚀 İzekip UI Web Sayfası', 'color: #667EEA; font-size: 18px; font-weight: bold;');
console.log('%cModern C++ GUI Kütüphanesi', 'color: #764BA2; font-size: 14px;');
console.log('%cGitHub: https://github.com/izekiptenknoloji-bit', 'color: #3498DB; font-size: 12px;');

// ===== PERFORMANCE MONITORING =====
window.addEventListener('load', () => {
    const perfData = window.performance.timing;
    const pageLoadTime = perfData.loadEventEnd - perfData.navigationStart;
    console.log(`⏱️  Sayfa Yükleme Süresi: ${pageLoadTime}ms`);
});

// ===== KEYBOARD SHORTCUTS =====
document.addEventListener('keydown', (e) => {
    // Ctrl+K: Ana sayfaya git
    if (e.ctrlKey && e.key === 'k') {
        e.preventDefault();
        window.scrollTo({ top: 0, behavior: 'smooth' });
    }

    // Ctrl+Shift+D: Dark mode toggle
    if (e.ctrlKey && e.shiftKey && e.key === 'd') {
        e.preventDefault();
        document.body.classList.toggle('dark-mode');
    }
});

// ===== ANALYTICS EVENT TRACKING =====
function trackEvent(eventName, eventData) {
    console.log(`📊 Event: ${eventName}`, eventData);
    // Google Analytics, Mixpanel, vb. integrasyon burada olabilir
}

// Feature card click tracking
document.querySelectorAll('.feature-card').forEach(card => {
    card.addEventListener('click', () => {
        const feature = card.getAttribute('data-feature');
        trackEvent('feature_viewed', { feature: feature });
    });
});

// Tab click tracking
document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        const tab = btn.getAttribute('data-tab');
        trackEvent('tab_clicked', { tab: tab });
    });
});

// ===== UTILITY FUNCTIONS =====

// Elemanın viewport'ta olup olmadığını kontrol et
function isInViewport(element) {
    const rect = element.getBoundingClientRect();
    return (
        rect.top >= 0 &&
        rect.left >= 0 &&
        rect.bottom <= (window.innerHeight || document.documentElement.clientHeight) &&
        rect.right <= (window.innerWidth || document.documentElement.clientWidth)
    );
}

// Debounce function (performans için)
function debounce(func, wait) {
    let timeout;
    return function executedFunction(...args) {
        const later = () => {
            clearTimeout(timeout);
            func(...args);
        };
        clearTimeout(timeout);
        timeout = setTimeout(later, wait);
    };
}

// Throttle function
function throttle(func, limit) {
    let inThrottle;
    return function(...args) {
        if (!inThrottle) {
            func.apply(this, args);
            inThrottle = true;
            setTimeout(() => inThrottle = false, limit);
        }
    };
}

// Smooth scroll helper
function smoothScroll(elementId) {
    const element = document.getElementById(elementId);
    if (element) {
        element.scrollIntoView({ behavior: 'smooth' });
    }
}

// ===== INITIALIZATION =====
console.log('✅ İzekip UI Web Sayfası Hazır!');
