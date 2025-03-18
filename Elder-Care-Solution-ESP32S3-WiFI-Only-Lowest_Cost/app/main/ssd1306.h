
/**
 * @file ssd1306.h
 * @defgroup drivers ssd1306
 * @{
 *
 * ESP-IDF driver for ssd1306 display panel
 *
 * Copyright (c) 2024 Eric Gionet (gionet.c.eric@gmail.com)
 *
 * MIT Licensed as described in the file LICENSE
 */
#ifndef __SSD1306_H__
#define __SSD1306_H__

/**
 * dependency includes
 */

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>
#include <driver/i2c_master.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * public constant definitions
 */

#define I2C_SSD1306_DEV_CLK_SPD           		UINT32_C(400000) //!< ssd1306 I2C default clock frequency (100KHz)

#define I2C_SSD1306_DEV_ADDR               		UINT8_C(0x3c)   //!< ssd1306 I2C address

#define I2C_XFR_TIMEOUT_MS						500
/**
 * public macro definitions
 */

/**
 * @brief Macro that initializes `i2c_ssd1306_config_t` to default configuration settings for a 128x32 display.
 */
#define I2C_SSD1306_128x32_CONFIG_DEFAULT 	{				\
    .i2c_address  				= I2C_SSD1306_DEV_ADDR,		\
	.i2c_clock_speed    		= I2C_SSD1306_DEV_CLK_SPD,  \
    .panel_size                 = I2C_SSD1306_PANEL_128x32,	\
    .offset_x                   = 0,						\
    .flip_enabled               = false }	

/**
 * @brief Macro that initializes `i2c_ssd1306_config_t` to default configuration settings for a 128x64 display.
 */
#define I2C_SSD1306_128x64_CONFIG_DEFAULT 	{				\
    .i2c_address  				= I2C_SSD1306_DEV_ADDR,		\
	.i2c_clock_speed    		= I2C_SSD1306_DEV_CLK_SPD,  \
    .panel_size                 = I2C_SSD1306_PANEL_128x64,	\
    .offset_x                   = 0,						\
    .flip_enabled               = false }

/*
 * enumerator and structure declarations
*/

/**
 * @brief SSD1306 scroll step in terms of frame frequency enumerator definition.
 * 
 */
typedef enum i2c_ssd1306_scroll_frames_e {
	I2C_SSD1306_SCROLL_5_FRAMES 	= 0b000,
	I2C_SSD1306_SCROLL_64_FRAMES 	= 0b001,
	I2C_SSD1306_SCROLL_128_FRAMES 	= 0b010,
	I2C_SSD1306_SCROLL_256_FRAMES	= 0b011,
	I2C_SSD1306_SCROLL_3_FRAMES 	= 0b100,
	I2C_SSD1306_SCROLL_4_FRAMES 	= 0b101,
	I2C_SSD1306_SCROLL_25_FRAMES 	= 0b110,
	I2C_SSD1306_SCROLL_2_FRAMES 	= 0b111
} i2c_ssd1306_scroll_frames_t;

/**
 * @brief SSD1306 scroll types enumerator definition.
 * 
 */
typedef enum i2c_ssd1306_scroll_types_e {
	I2C_SSD1306_SCROLL_RIGHT	= 1,
	I2C_SSD1306_SCROLL_LEFT 	= 2,
	I2C_SSD1306_SCROLL_DOWN 	= 3,
	I2C_SSD1306_SCROLL_UP 		= 4,
	I2C_SSD1306_SCROLL_STOP 	= 5
} i2c_ssd1306_scroll_types_t;

/**
 * @brief SSD1306 panel sizes enumerator definition.
 * 
 */
typedef enum i2c_ssd1306_panel_sizes_e {
	I2C_SSD1306_PANEL_128x32 = 1,
	I2C_SSD1306_PANEL_128x64 = 2
} i2c_ssd1306_panel_sizes_t;

/**
 * @brief SSD1306 page structure definition.
 */
typedef struct i2c_ssd1306_page_s {
	uint8_t segment[128];		/*!< page segment data to display */
} i2c_ssd1306_page_t;

/**
 * @brief SSD1306 configuration structure definition.
 */
typedef struct i2c_ssd1306_config_s {
	uint16_t                    i2c_address;    /*!< ssd1306 i2c device address */
    uint32_t                    i2c_clock_speed;/*!< ssd1306 i2c device scl clock speed  */
	i2c_ssd1306_panel_sizes_t	panel_size;		/*!< ssd1306 panel size */
	uint8_t						offset_x;	    /*!< ssd1306 x-axis offset */
	bool						flip_enabled;   /*!< ssd1306 displayed information is flipped when true */
	bool						display_enabled;/*!< ssd1306 display is on when true otherwise it is sleeping */
} i2c_ssd1306_config_t;

/**
 * @brief SSD1306 context structure.
 */
struct i2c_ssd1306_context_t {
	i2c_ssd1306_config_t 	 dev_config;    /*!< ssd1306 device configuration */
    i2c_master_dev_handle_t  i2c_handle;    /*!< ssd1306 i2c device handle */
	uint8_t				width;				/*!< ssd1306 width of display panel */
	uint8_t 			height;				/*!< ssd1306 height display panel */
	bool				scroll_enabled;		/*!< ssd1306 scroll enabled when true */
	uint8_t				scroll_start;		/*!< ssd1306 start page of scroll */
	uint8_t				scroll_end;			/*!< ssd1306 end page of scroll */
	int8_t			    scroll_direction;   /*!< ssd1306 scroll direction */
	uint8_t				pages;				/*!< ssd1306 number of pages supported by display panel */
	i2c_ssd1306_page_t	page[8];			/*!< ssd1306 pages of segment data to display */
};

/**
 * @brief SSD1306 context structure definition.
 */
typedef struct i2c_ssd1306_context_t i2c_ssd1306_context_t;

/**
 * @brief SSD1306 handle stucture definition.
 */
typedef struct i2c_ssd1306_context_t* i2c_ssd1306_handle_t;

/**
 * public function and subroutine declarations
 */

/**
 * @brief Turns SSD1306 display panel on.
 * 
 * @param handle SSD1306 device handle.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_enable_display(i2c_ssd1306_handle_t handle);

/**
 * @brief Turns SSD1306 display panel off.
 * 
 * @param handle SSD1306 device handle.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_disable_display(i2c_ssd1306_handle_t handle);

/**
 * @brief Displays segment data for each page supported by the SSD1306 display panel.
 * 
 * @param handle SSD1306 device handle.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_display_pages(i2c_ssd1306_handle_t handle);

/**
 * @brief Sets segment data for each page supported by the SSD1306 display panel.
 * 
 * @param handle SSD1306 device handle.
 * @param buffer Segment data in 128-byte blocks by page.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_set_pages(i2c_ssd1306_handle_t handle, uint8_t *buffer);

/**
 * @brief Gets segment data for each page supported by the SSD1306 display panel.
 * 
 * @param handle SSD1306 device handle.
 * @param buffer Segment data in 128-byte blocks by page.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_get_pages(i2c_ssd1306_handle_t handle, uint8_t *buffer);

/**
 * @brief Sets SSD1306 page segment data for a pixel.
 * 
 * @note Call `i2c_ssd1306_display_pages` to display the pixel.
 * 
 * @param handle SSD1306 device handle.
 * @param xpos X-axis position of the pixel.
 * @param ypos Y-axis position of the pixel.
 * @param invert Pixel is inverted when true.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_set_pixel(i2c_ssd1306_handle_t handle, int16_t xpos, int16_t ypos, bool invert);

/**
 * @brief Sets SSD1306 pages and segments data for a line.
 * 
 * @note Call `i2c_ssd1306_display_pages` to display the line.
 * 
 * @param handle SSD1306 device handle.
 * @param x1 X-axis start position of the line.
 * @param y1 Y-axis start position of the line.
 * @param x2 X-axis end position of the line.
 * @param y2 Y-axis end position of the line.
 * @param invert Line is inverted when true.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_set_line(i2c_ssd1306_handle_t handle, int16_t x1, int16_t y1, int16_t x2, int16_t y2,  bool invert);

/**
 * @brief Sets SSD1306 pages and segments data for a circle.
 * 
 * @note Call `i2c_ssd1306_display_pages` to display the circle.
 * 
 * @param handle SSD1306 device handle.
 * @param x0 X-axis start position of the circle.
 * @param y0 Y-axis start position of the circle.
 * @param r Radius of the circle.
 * @param invert Circle is inverted when true.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_set_circle(i2c_ssd1306_handle_t handle, int16_t x0, int16_t y0, int16_t r, bool invert);

/**
 * @brief Sets contrast of the SSD1306 display panel.
 * 
 * @param handle SSD1306 device handle.
 * @param contrast Contrast of information being displayed (0 to 255).
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_set_display_contrast(i2c_ssd1306_handle_t handle, uint8_t contrast);

/**
 * @brief Displays a bitmap on the SSD1306.
 * 
 * @param handle SSD1306 device handle.
 * @param xpos X-axis position of the bitmap.
 * @param ypos Y-axis position of the bitmap.
 * @param bitmap Bitmap data.
 * @param width Width of the bitmap.
 * @param height Height of the bitmap
 * @param invert Bitmap is inverted when true.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_display_bitmap(i2c_ssd1306_handle_t handle, int16_t xpos, int16_t ypos, uint8_t *bitmap, uint8_t width, uint8_t height, bool invert);

/**
 * @brief Displays an image by page and segment on the SSD1306.
 * 
 * @param handle SSD1306 device handle.
 * @param page Index of page.
 * @param segment Index of segment data.
 * @param image Image data.
 * @param width Width of the image.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_display_image(i2c_ssd1306_handle_t handle, uint8_t page, uint8_t segment, uint8_t *image, uint8_t width);

/**
 * @brief Displays text by page on the SSD1306 with a maximum of 16-characters.
 * 
 * @param handle SSD1306 device handle.
 * @param page Index of page.
 * @param text Text characters.
 * @param text_len Length of text (16-characters maximum).
 * @param invert Text is inverted when true.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_display_text(i2c_ssd1306_handle_t handle, uint8_t page, char *text, uint8_t text_len, bool invert);

/**
 * @brief Displays text x2 larger by page on the SSD1306.
 * 
 * @note Text displayed uses 2-pages with a maximum of 8-characters.
 * 
 * @param handle SSD1306 device handle.
 * @param page Index of page.
 * @param text Text characters.
 * @param text_len Length of text (8-characters maximum).
 * @param invert Text is inverted when true.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_display_text_x2(i2c_ssd1306_handle_t handle, uint8_t page, char *text, uint8_t text_len, bool invert);

/**
 * @brief Displays text x3 larger by page on the SSD1306.
 * 
 * @note Text displayed uses 3-pages with a maximum of 5 characters.
 * 
 * @param handle SSD1306 device handle.
 * @param page Index of page.
 * @param text Text characters.
 * @param text_len Length of text (5-characters maximum).
 * @param invert Text is inverted when true.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_display_text_x3(i2c_ssd1306_handle_t handle, uint8_t page, char *text, uint8_t text_len, bool invert);

/**
 * @brief Clears a page from the SSD1306 display.
 * 
 * @param handle SSD1306 device handle.
 * @param page Index of page to clear from the display.
 * @param invert Background is inverted when true.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_clear_display_page(i2c_ssd1306_handle_t handle, uint8_t page, bool invert);

/**
 * @brief Clears the entire SSD1306 display.
 * 
 * @param handle SSD1306 device handle.
 * @param invert Background is inverted when true.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_clear_display(i2c_ssd1306_handle_t handle, bool invert);

/**
 * @brief Sets SSD1306 scroll orientation and frame frequency for hardware based scrolling text.
 * 
 * @note Call `i2c_ssd1306_display_text` to display hardware based scrolling text.
 * 
 * @param handle SSD1306 device handle.
 * @param scroll Scrolling orientation.
 * @param frame_frequency Frame rate of scrolling text.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_set_hardware_scroll(i2c_ssd1306_handle_t handle, i2c_ssd1306_scroll_types_t scroll, i2c_ssd1306_scroll_frames_t frame_frequency);

/**
 * @brief Sets SSD1306 start and end page for software based scrolling text.
 * 
 * @note Call `i2c_ssd1306_display_scroll_text` to display software based scrolling text.
 * 
 * @param handle SSD1306 device handle.
 * @param start Index of start page.
 * @param end Index of end page.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_set_software_scroll(i2c_ssd1306_handle_t handle, uint8_t start, uint8_t end);

/**
 * @brief Displays software based scrolling text on the SSD1306.
 * 
 * @param handle SSD1306 device handle.
 * @param text Text characters.
 * @param text_len Length of text.
 * @param invert Text is inverted when true.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_display_scroll_text(i2c_ssd1306_handle_t handle, char *text, uint8_t text_len, bool invert);

/**
 * @brief Clears software based scrolling text from SSD1306 display.
 * 
 * @param handle SSD1306 device handle.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_clear_scroll_display(i2c_ssd1306_handle_t handle);

/**
 * @brief Sets SSD1306 scroll orientation, start and end pages to wrap around the display.
 * 
 * @param handle SSD1306 device handle.
 * @param scroll Scrolling orientation.
 * @param start Index of page for left or right scroll, otherwise, height position for up or down scroll.
 * @param end Index of page for left or right scroll, otherwise, height position for up or down scroll.
 * @param delay Delay before information is display, a value 0 there is no wait, and nothing is displayed with a value of -1.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_set_display_wrap_arround(i2c_ssd1306_handle_t handle, i2c_ssd1306_scroll_types_t scroll, uint8_t start, uint8_t end, int8_t delay);

/**
 * @brief Copies bit from source to destination.
 * 
 * @param src 
 * @param src_bits 
 * @param dst 
 * @param dst_bits 
 * @return uint8_t 
 */
uint8_t i2c_ssd1306_copy_bit(uint8_t src, uint8_t src_bits, uint8_t dst, uint8_t dst_bits);

/**
 * @brief Inverts the buffer data.
 * 
 * @param buf Buffer data.
 * @param blen Length of buffer data.
 */
void i2c_ssd1306_invert_buffer(uint8_t *buf, size_t blen);

/**
 * @brief Flips the buffer data (upsidedown).
 * 
 * @param buf Buffer data.
 * @param blen Length of buffer data.
 */
void i2c_ssd1306_flip_buffer(uint8_t *buf, size_t blen);

/**
 * @brief Rotates 8-bits, as an example, 0x12 becomes 0x48.
 * 
 * @param ch1 8-bit value to rotate.
 * @return uint8_t rotated 8-bit value.
 */
uint8_t i2c_ssd1306_rotate_byte(uint8_t ch1);

/**
 * @brief SSD1306 display is faded out and cleared.
 * 
 * @param handle SSD1306 device handle.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_fadeout_display(i2c_ssd1306_handle_t handle);

/**
 * @brief Initializes an SSD1306 device onto the I2C master bus.
 *
 * @param[in] master_handle I2C master bus handle.
 * @param[in] ssd1306_config SSD1306 device configuration.
 * @param[out] ssd1306_handle SSD1306 device handle.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_init(i2c_master_bus_handle_t master_handle, const i2c_ssd1306_config_t *ssd1306_config, i2c_ssd1306_handle_t *ssd1306_handle);

/**
 * @brief Removes an SSD1306 device from master bus.
 *
 * @param[in] handle SSD1306 device handle.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_remove(i2c_ssd1306_handle_t handle);

/**
 * @brief Removes an SSD1306 device from master bus and frees handle.
 * 
 * @param handle SSD1306 device handle.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t i2c_ssd1306_delete(i2c_ssd1306_handle_t handle);


#ifdef __cplusplus
}
#endif

/**@}*/

#endif /* __SSD1306_H__ */