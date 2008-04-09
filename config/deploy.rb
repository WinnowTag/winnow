set :application, "classifier"
set :repository,  "set your repository location here"

# If you aren't deploying to /u/apps/#{application} on the target
# servers (which is the default), you can specify the actual location
# via the :deploy_to variable:
set :deploy_to, "/home/winnow/classifier.build"
set :user, "winnow"

ssh_options[:port] = 65000

if ENV['dest'] == 'all'
  role :app, "mindloom.org"
  role :app, "wizztag.org"
elsif ENV['dest'] == 'beta'
  role :app, "mindloom.org"
else
  role :app, "wizztag.org"
end

namespace :deploy do
  def tarball_name
    "#{dirname}.tar.gz"
  end
  
  def dirname
    if ENV['version']
      "classifier-#{ENV['version']}"
    else
      raise ArgumentError, "You must specify the version to deploy using version=X.Y.Z"
    end
  end
  
  desc "Deploy a version of the classifier"
  task :default do
    transaction do
      update
      build
      install
      restart
    end
  end
  
  desc "Push up the latest version"
  task :update do
    system("rm -f src/git_revision.h")
    system("make dist")
    md5 = `md5 #{tarball_name}`
    put(File.read(tarball_name), "#{deploy_to}/#{tarball_name}")
    put(md5, "#{deploy_to}/#{tarball_name}.md5")
    run("cd #{deploy_to} && md5sum --check #{deploy_to}/#{tarball_name}.md5 && rm -Rf #{dirname} && tar -zxf #{tarball_name}")    
  end  
      
  desc "Build the classifier"
  task :build do
    run("cd #{deploy_to}/#{dirname} && ./configure && make")
  end
  
  desc "Install the classifier"
  task :install do
    sudo("make -C #{deploy_to}/#{dirname} install")
  end

  desc "Restart the classifier"
  task :restart do
    sudo("god restart classifier")
  end
  
  task :setup do
    run("mkdir -p #{deploy_to}")
  end
end
