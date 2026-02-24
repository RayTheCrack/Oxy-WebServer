// 表单提交处理
document.addEventListener('DOMContentLoaded', function() {
    // 登录表单提交
    const loginForm = document.getElementById('loginForm');
    if (loginForm) {
        loginForm.addEventListener('submit', async function(e) {
            e.preventDefault();
            
            // 获取表单数据
            const username = document.querySelector('input[name="username"]').value.trim();
            const password = document.querySelector('input[name="password"]').value.trim();
            
            // 验证
            if (!username || !password) {
                alert('用户名和密码不能为空！');
                return;
            }
            
            const formData = new FormData(loginForm);
            const params = new URLSearchParams(formData);
            
            try {
                const response = await fetch('/login', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/x-www-form-urlencoded',
                    },
                    body: params.toString()
                });
                
                if (response.ok) {
                    window.location.href = '/welcome';
                } else {
                    alert('登录失败，请检查用户名和密码');
                    window.location.href = '/login';
                }
            } catch (error) {
                console.error('登录失败:', error);
                alert('登录出错，请稍后重试');
            }
        });
    }
    
    // 注册表单提交
    const regForm = document.getElementById('registerForm') || document.getElementById('regForm');
    if (regForm) {
        regForm.addEventListener('submit', async function(e) {
            e.preventDefault();
            
            // 获取表单数据
            const username = document.querySelector('input[name="username"]').value.trim();
            const password = document.querySelector('input[name="password"]').value.trim();
            const password2 = document.querySelector('input[name="password2"]') ? 
                             document.querySelector('input[name="password2"]').value.trim() : password;
            
            // 验证
            if (!username || !password) {
                alert('用户名和密码不能为空！');
                return;
            }
            
            if (password2 && password !== password2) {
                alert('两次输入的密码不一致！');
                return;
            }
            
            if (password.length < 6) {
                alert('密码长度不能少于6位！');
                return;
            }
            
            const formData = new FormData(regForm);
            const params = new URLSearchParams(formData);
            
            try {
                const response = await fetch('/register', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/x-www-form-urlencoded',
                    },
                    body: params.toString()
                });
                
                if (response.ok) {
                    alert('注册成功！请登录');
                    window.location.href = '/login';
                } else {
                    alert('注册失败，用户名可能已存在');
                    window.location.href = '/register';
                }
            } catch (error) {
                console.error('注册失败:', error);
                alert('注册出错，请稍后重试');
            }
        });
    }
    
    // 加载图片库
    if (document.getElementById('gallery')) {
        loadGallery();
    }
    
    // 加载视频库
    if (document.getElementById('videoGallery')) {
        loadVideos();
    }
});

// 动态加载图片库
async function loadGallery() {
    try {
        const gallery = document.getElementById('gallery');
        const noImages = document.getElementById('noImages');
        
        // 预定义的图片路径列表
        const baseUrls = [
            '/image/pic1.jpg', '/image/pic2.jpg', '/image/pic3.jpg',
            '/image/pic4.jpg', '/image/demo1.jpg', '/image/demo2.jpg'
        ];
        
        let foundImages = 0;
        
        // 检测并加载存在的图片
        for (const url of baseUrls) {
            try {
                const response = await fetch(url, {method: 'HEAD', cache: 'no-cache'});
                if (response.ok) {
                    foundImages++;
                    const name = url.split('/').pop();
                    gallery.innerHTML += `
                        <div style="background:var(--bg-secondary);border:1px solid var(--border);border-radius:8px;overflow:hidden;transition:all 0.3s ease" 
                             onmouseover="this.style.borderColor='var(--accent-red)'; this.style.boxShadow='0 5px 15px rgba(230,57,70,0.2)'" 
                             onmouseout="this.style.borderColor='var(--border)'; this.style.boxShadow='none'">
                            <img src="${url}" style="width:100%;height:200px;object-fit:cover;display:block;cursor:pointer" alt="${name}" onclick="window.open('${url}', '_blank')">
                            <div style="padding:0.75rem">
                                <p style="margin:0;color:var(--text-secondary);font-size:0.9rem;text-align:center;white-space:nowrap;overflow:hidden;text-overflow:ellipsis">${name}</p>
                            </div>
                        </div>
                    `;
                }
            } catch (e) {
                // 图片不存在或加载失败，跳过
            }
        }
        
        if (foundImages === 0) {
            gallery.style.display = 'none';
            noImages.style.display = 'block';
        } else {
            gallery.style.display = 'grid';
            noImages.style.display = 'none';
        }
    } catch (error) {
        console.error('加载图片库失败:', error);
    }
}

// 动态加载视频库
async function loadVideos() {
    try {
        const videoGallery = document.getElementById('videoGallery');
        const noVideos = document.getElementById('noVideos');
        
        // 预定义的视频路径列表
        const baseUrls = [
            '/video/xxx.mp4', '/video/demo1.mp4', '/video/demo2.mp4',
            '/video/sample.mp4', '/video/test.mp4', '/video/movie.mp4'
        ];
        
        let foundVideos = 0;
        
        // 检测并加载存在的视频
        for (const url of baseUrls) {
            try {
                const response = await fetch(url, {method: 'HEAD', cache: 'no-cache'});
                if (response.ok) {
                    foundVideos++;
                    const name = url.split('/').pop();
                    videoGallery.innerHTML += `
                        <div style="background:var(--bg-secondary);border:1px solid var(--border);border-radius:8px;overflow:hidden;transition:all 0.3s ease" 
                             onmouseover="this.style.borderColor='var(--accent-red)'; this.style.boxShadow='0 5px 15px rgba(230,57,70,0.2)'" 
                             onmouseout="this.style.borderColor='var(--border)'; this.style.boxShadow='none'">
                            <video style="width:100%;height:200px;background:#000;object-fit:cover;display:block" controls preload="metadata">
                                <source src="${url}" type="video/mp4">
                            </video>
                            <div style="padding:0.75rem">
                                <p style="margin:0;color:var(--text-secondary);font-size:0.9rem;text-align:center;white-space:nowrap;overflow:hidden;text-overflow:ellipsis">${name}</p>
                            </div>
                        </div>
                    `;
                }
            } catch (e) {
                // 视频不存在或加载失败，跳过
            }
        }
        
        if (foundVideos === 0) {
            videoGallery.style.display = 'none';
            noVideos.style.display = 'block';
        } else {
            videoGallery.style.display = 'grid';
            noVideos.style.display = 'none';
        }
    } catch (error) {
        console.error('加载视频库失败:', error);
    }
}
