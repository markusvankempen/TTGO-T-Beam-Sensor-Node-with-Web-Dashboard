#!/usr/bin/env python3
"""
TTGO T-Beam Web UI Screenshot Capture Script

Author: Markus van Kempen
Email: markus.van.kempen@gmail.com
Organization: Research | Floor 7½ 🏢🤏

Description:
    Automatically captures screenshots of all TTGO T-Beam web dashboard pages
    using Selenium WebDriver. Screenshots are saved to the 'screenshots' directory.

Requirements:
    pip install selenium
    
    Chrome/Chromium browser installed
    ChromeDriver installed (or use webdriver-manager):
    pip install webdriver-manager

Usage:
    python3 capture_screenshots.py
    
    Or specify custom device IP:
    python3 capture_screenshots.py --ip 192.168.1.100
"""

import os
import sys
import time
import argparse
from selenium import webdriver
from selenium.webdriver.chrome.service import Service
from selenium.webdriver.chrome.options import Options
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC

# Try to import webdriver-manager for automatic ChromeDriver management
try:
    from webdriver_manager.chrome import ChromeDriverManager
    USE_WEBDRIVER_MANAGER = True
except ImportError:
    USE_WEBDRIVER_MANAGER = False
    print("⚠️  webdriver-manager not installed. Using system ChromeDriver.")
    print("   Install with: pip install webdriver-manager")

# Default device IP
DEFAULT_DEVICE_IP = "10.0.4.80"

# Pages to capture
PAGES = [
    {"path": "/", "name": "dashboard", "title": "Dashboard"},
    {"path": "/settings", "name": "settings", "title": "Settings"},
    {"path": "/config", "name": "config", "title": "LoRa Configuration"},
    {"path": "/payload-info", "name": "payload-info", "title": "Payload Info"},
    {"path": "/diagnostics", "name": "diagnostics", "title": "Diagnostics"},
    {"path": "/debug", "name": "debug", "title": "Debug Console"},
    {"path": "/about", "name": "about", "title": "About"},
]

# Screenshot settings
SCREENSHOT_WIDTH = 1280
SCREENSHOT_HEIGHT = 1024  # Increased height for more content
WAIT_TIME = 2  # seconds to wait for page load
OUTPUT_DIR = "screenshots"
FULL_PAGE_SCREENSHOT = True  # Capture entire page including scrollable content


def setup_driver():
    """Initialize Chrome WebDriver with appropriate options"""
    chrome_options = Options()
    chrome_options.add_argument(f"--window-size={SCREENSHOT_WIDTH},{SCREENSHOT_HEIGHT}")
    chrome_options.add_argument("--headless")  # Run in headless mode
    chrome_options.add_argument("--disable-gpu")
    chrome_options.add_argument("--no-sandbox")
    chrome_options.add_argument("--disable-dev-shm-usage")
    
    # Initialize driver
    if USE_WEBDRIVER_MANAGER:
        service = Service(ChromeDriverManager().install())
        driver = webdriver.Chrome(service=service, options=chrome_options)
    else:
        # Use system ChromeDriver
        driver = webdriver.Chrome(options=chrome_options)
    
    return driver


def capture_page(driver, base_url, page_info, output_dir):
    """Capture screenshot of a single page"""
    url = f"{base_url}{page_info['path']}"
    filename = f"{page_info['name']}.png"
    filepath = os.path.join(output_dir, filename)
    
    print(f"📸 Capturing {page_info['title']}...")
    print(f"   URL: {url}")
    
    try:
        # Navigate to page
        driver.get(url)
        
        # Wait for page to load
        time.sleep(WAIT_TIME)
        
        # Wait for body element to be present
        WebDriverWait(driver, 10).until(
            EC.presence_of_element_located((By.TAG_NAME, "body"))
        )
        
        # Additional wait for dynamic content (like maps)
        if page_info['name'] == 'dashboard':
            time.sleep(3)  # Extra time for map to load
        
        if FULL_PAGE_SCREENSHOT:
            # Get the full page height
            total_height = driver.execute_script("return document.body.scrollHeight")
            viewport_height = driver.execute_script("return window.innerHeight")
            
            print(f"   Page height: {total_height}px, Viewport: {viewport_height}px")
            
            # Set window size to capture full page
            driver.set_window_size(SCREENSHOT_WIDTH, total_height)
            time.sleep(0.5)  # Let browser adjust
            
            # Capture full page screenshot
            driver.save_screenshot(filepath)
            print(f"   ✓ Full-page screenshot saved to {filepath}")
        else:
            # Capture viewport screenshot
            driver.save_screenshot(filepath)
            print(f"   ✓ Saved to {filepath}")
        
        return True
        
    except Exception as e:
        print(f"   ✗ Error: {e}")
        return False


def main():
    """Main function to capture all screenshots"""
    parser = argparse.ArgumentParser(
        description="Capture screenshots of TTGO T-Beam web dashboard"
    )
    parser.add_argument(
        "--ip",
        default=DEFAULT_DEVICE_IP,
        help=f"Device IP address (default: {DEFAULT_DEVICE_IP})"
    )
    parser.add_argument(
        "--output",
        default=OUTPUT_DIR,
        help=f"Output directory (default: {OUTPUT_DIR})"
    )
    parser.add_argument(
        "--viewport-only",
        action="store_true",
        help="Capture viewport only instead of full page (faster but may cut content)"
    )
    args = parser.parse_args()
    
    # Override global setting if viewport-only is specified
    global FULL_PAGE_SCREENSHOT
    if args.viewport_only:
        FULL_PAGE_SCREENSHOT = False
    
    base_url = f"http://{args.ip}"
    output_dir = args.output
    
    print("╔════════════════════════════════════════════════════════════╗")
    print("║     TTGO T-Beam Web UI Screenshot Capture Tool            ║")
    print("║  Author: Markus van Kempen | Floor 7½ 🏢🤏                ║")
    print("╚════════════════════════════════════════════════════════════╝")
    print()
    print(f"Device URL: {base_url}")
    print(f"Output directory: {output_dir}")
    print(f"Screenshot width: {SCREENSHOT_WIDTH}px")
    print(f"Full-page capture: {'Enabled' if FULL_PAGE_SCREENSHOT else 'Disabled'}")
    print(f"Pages to capture: {len(PAGES)}")
    print()
    
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    print(f"✓ Output directory ready: {output_dir}")
    print()
    
    # Initialize WebDriver
    print("🚀 Initializing Chrome WebDriver...")
    try:
        driver = setup_driver()
        print("✓ WebDriver initialized")
        print()
    except Exception as e:
        print(f"✗ Failed to initialize WebDriver: {e}")
        print()
        print("Troubleshooting:")
        print("1. Install Chrome/Chromium browser")
        print("2. Install ChromeDriver:")
        print("   - macOS: brew install chromedriver")
        print("   - Or use: pip install webdriver-manager")
        print("3. Ensure Chrome and ChromeDriver versions match")
        sys.exit(1)
    
    # Capture screenshots
    success_count = 0
    failed_pages = []
    
    for page in PAGES:
        if capture_page(driver, base_url, page, output_dir):
            success_count += 1
        else:
            failed_pages.append(page['title'])
        print()
    
    # Cleanup
    driver.quit()
    
    # Summary
    print("═" * 60)
    print(f"✓ Capture complete!")
    print(f"  Successful: {success_count}/{len(PAGES)}")
    if failed_pages:
        print(f"  Failed: {', '.join(failed_pages)}")
    print(f"  Screenshots saved to: {output_dir}/")
    print()
    print("Next steps:")
    print("1. Review screenshots in the output directory")
    print("2. Add screenshots to README.md")
    print("3. Commit and push to GitHub")
    print("═" * 60)


if __name__ == "__main__":
    main()

# Made with Bob
